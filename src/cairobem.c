/* Cairo back-end management */
#include <R.h>
#include <Rinternals.h>

#include "backend.h"

static char *types[64];

static struct bed_list_st {
  Rcairo_backend_def* bed;
  struct bed_list_st* next;
} root;

void Rcairo_register_backend(Rcairo_backend_def* def) {
  struct bed_list_st *l=&root;
  while (l->bed && l->next) {
    if (l->bed == def) return; /* nothing to do if we have this be alreay */
    l = l->next;
  }
  if (l->bed) {
    l->next = (struct bed_list_st*) malloc(sizeof(struct bed_list_st));
    l = l->next;
    l->next = 0;
  }
  l->bed = def;
  { /* collect types list */
    char **c = types;
    char **d = def->types;
    while (*c) c++;
    while (*d) {
      *c=*d; c++; d++;
      if (c-types > 48) break;
    }
  }
}

int Rcairo_type_supported(char *type) {
  char **c = types;
  if (!type) return 0;
  while (*c) {
    if (!strcmp(type, *c)) return 1;
    c++;
  }
  return 0;
}

SEXP Rcairo_supported_types() {
  char **c = types;
  int i = 0;
  SEXP res;
  while (*c) { c++; i++; }
  PROTECT(res=allocVector(STRSXP, i));
  i = 0; c = types;
  while (*c) {
    SET_VECTOR_ELT(res, i, mkChar(*c));
    c++; i++;
  }
  UNPROTECT(1);
  return res;
}

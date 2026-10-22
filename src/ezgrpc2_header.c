#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "ezgrpc2_header.h"

ezgrpc2_header *ezgrpc2_header_new(const u8 *name, size_t nlen, const u8 *value, size_t vlen) {
  ezgrpc2_header *ezheader = malloc(sizeof(*ezheader));
  if (ezheader == nullptr)
    return nullptr;

  ezheader->name = malloc(nlen + 1);
  ezheader->value = malloc(vlen + 1);
  if (ezheader->name == nullptr || ezheader->value == nullptr) {
    free(ezheader->name);
    free(ezheader->value);
    free(ezheader);
    return nullptr;
  }

  memcpy(ezheader->name, name, nlen);
  ezheader->name[nlen] = '\0';
  memcpy(ezheader->value, value, vlen);
  ezheader->value[vlen] = '\0';
  ezheader->namelen = nlen;
  ezheader->valuelen = vlen;

  return ezheader;
}

void ezgrpc2_header_free(ezgrpc2_header *ezheader) {
  if (ezheader == nullptr)
    return;
  free(ezheader->name);
  free(ezheader->value);
  free(ezheader);
}

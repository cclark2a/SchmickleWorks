#pragma once

#define Curl_safefree(ptr) \
  do { free((ptr)); (ptr) = NULL;} WHILE_FALSE
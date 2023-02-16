#pragma once
#include "belnet_export.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /// change our network id globally across all contexts
  void EXPORT
  belnet_set_netid(const char*);

  /// get our current netid
  /// must be free()'d after use
  const char* EXPORT
  belnet_get_netid();

  /// set log level
  /// possible values: trace, debug, info, warn, error, none
  /// return 0 on success
  /// return non zero on fail
  int EXPORT
  belnet_log_level(const char*);

  typedef void (*belnet_logger_func)(const char*, void*);

  /// set a custom logger function
  void EXPORT
  belnet_set_logger(belnet_logger_func func, void* user);

  /// @brief take in hex and turn it into base32z
  /// @return value must be free()'d later
  char* EXPORT
  belnet_hex_to_base32z(const char* hex);

#ifdef __cplusplus
}
#endif

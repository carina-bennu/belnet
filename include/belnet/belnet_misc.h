#pragma once
#include "belnet_export.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /// change our network id globally across all contexts
  void EXPORT
  belnet_set_netid(const char* netid);

  /// get our current netid
  /// must be free()'d after use
  const char* EXPORT
  belnet_get_netid();

  /// set log level
  /// possible values: trace, debug, info, warn, critical, error, none
  /// return 0 on success
  /// return non zero on fail
  int EXPORT
  belnet_log_level(const char* level);

  /// Function pointer to invoke with belnet log messages
  typedef void (*belnet_logger_func)(const char* message, void* context);

  /// Optional function to call when flushing belnet log messages; can be NULL if flushing is not
  /// meaningful for the logging system.
  typedef void (*belnet_logger_sync)(void* context);

  /// set a custom logger function; it is safe (and often desirable) to call this before calling
  /// initializing belnet via belnet_context_new.
  void EXPORT
  belnet_set_syncing_logger(belnet_logger_func func, belnet_logger_sync sync, void* context);

  /// shortcut for calling `belnet_set_syncing_logger` with a NULL sync

  void EXPORT
  belnet_set_logger(belnet_logger_func func, void* context);

  /// @brief take in hex and turn it into base32z
  /// @return value must be free()'d later
  char* EXPORT
  belnet_hex_to_base32z(const char* hex);

#ifdef __cplusplus
}
#endif

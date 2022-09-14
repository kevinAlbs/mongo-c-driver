include (CheckSymbolExists)

if (ENABLE_SRV STREQUAL ON OR ENABLE_SRV STREQUAL AUTO)
   if (WIN32)
      set (FOUND_RESOLV_FUNCTIONS 1)
      set (RESOLV_LIBRARIES Dnsapi)
      set (MONGOC_HAVE_DNSAPI 1)
      set (MONGOC_HAVE_RES_NSEARCH 0)
      set (MONGOC_HAVE_RES_NDESTROY 0)
      set (MONGOC_HAVE_RES_NCLOSE 0)
      set (MONGOC_HAVE_RES_SEARCH 0)
   else ()
      set (MONGOC_HAVE_DNSAPI 0)
      # Thread-safe DNS query function for _mongoc_client_get_srv.
      # Could be a macro, not a function, so use check_symbol_exists.
      # check_symbol_exists (res_nsearch resolv.h MONGOC_HAVE_RES_NSEARCH)
      if (MONGOC_HAVE_RES_NSEARCH)
         set (FOUND_RESOLV_FUNCTIONS 1)
         set (RESOLV_LIBRARIES resolv)
         set (MONGOC_HAVE_RES_SEARCH 0)

         # We have res_nsearch. Call res_ndestroy (BSD/Mac) or res_nclose (Linux)?
         check_symbol_exists (res_ndestroy resolv.h MONGOC_HAVE_RES_NDESTROY)
         if (MONGOC_HAVE_RES_NDESTROY)
            set (MONGOC_HAVE_RES_NCLOSE 0)
         else ()
            set (MONGOC_HAVE_RES_NDESTROY 0)
            check_symbol_exists (res_nclose resolv.h MONGOC_HAVE_RES_NCLOSE)
            if (NOT MONGOC_HAVE_RES_NCLOSE)
               set (MONGOC_HAVE_RES_NCLOSE 0)
            endif ()
         endif ()
      else ()
         set (MONGOC_HAVE_RES_NSEARCH 0)
         set (MONGOC_HAVE_RES_NDESTROY 0)
         set (MONGOC_HAVE_RES_NCLOSE 0)

         # Check for the thread-unsafe function res_search.

         # Optionally check for libresolv. FreeBSD defines res_search in libc, not libresolv.
         find_library(RESOLV_LIBRARIES resolv)

         include (CMakePushCheckState)
         include (CheckCSourceCompiles)
         cmake_push_check_state (RESET)
            list (APPEND CMAKE_REQUIRED_LIBRARIES ${RESOLV_LIBRARIES})
            # Do not use check_symbol_exists. FreeBSD requires multiple headers to compile a test program with res_search.
            check_c_source_compiles ([[
               #include <sys/types.h>
               #include <netinet/in.h>
               #include <arpa/nameser.h>
               #include <resolv.h>
            
               int main (void) {
                  int len;
                  unsigned char reply[1024];
                  len = res_search("example.com", ns_c_in, ns_t_srv, reply, sizeof(reply));
                  return 0;
               }
            ]] MONGOC_HAVE_RES_SEARCH)
         cmake_pop_check_state ()

         if (MONGOC_HAVE_RES_SEARCH)
            set (FOUND_RESOLV_FUNCTIONS 1)
            set (RESOLV_LIBRARIES resolv)
         else ()
            set (MONGOC_HAVE_RES_SEARCH 0)
         endif ()
      endif ()
   endif ()
else ()
   # ENABLE_SRV disabled, set default values for defines.
   set (MONGOC_HAVE_DNSAPI 0)
   set (MONGOC_HAVE_RES_NSEARCH 0)
   set (MONGOC_HAVE_RES_NDESTROY 0)
   set (MONGOC_HAVE_RES_NCLOSE 0)
   set (MONGOC_HAVE_RES_SEARCH 0)  
endif ()

if (ENABLE_SRV STREQUAL ON AND NOT FOUND_RESOLV_FUNCTIONS)
   message (
      FATAL_ERROR
      "Cannot find libresolv or dnsapi. Try setting ENABLE_SRV=OFF")
endif ()

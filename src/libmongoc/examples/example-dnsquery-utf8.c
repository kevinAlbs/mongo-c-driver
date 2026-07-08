/*
 * example-dnsquery-utf8.c
 *
 * Windows-only. Demonstrates that DnsQuery_UTF8 returns UTF-8 (narrow, char)
 * strings in the returned DNS_RECORD, even when built with UNICODE/_UNICODE
 * defined (which makes PDNS_RECORD -> PDNS_RECORDW and its string fields
 * statically typed PWSTR). This is the behavior mongoc-client.c relies on for
 * SRV/TXT resolution (see CDRIVER-6346).
 *
 * For each returned record's string field, this prints:
 *   - the value via printf("%s", ...) reinterpreted as const char*, and
 *   - a hex dump of the raw bytes.
 *
 * Interpretation:
 *   - If the bytes are contiguous ASCII with NO interleaved 0x00, the data is
 *     UTF-8/narrow (what DnsQuery_UTF8 documents).
 *   - If every other byte were 0x00, the data would be UTF-16/wide -- it is not.
 *
 * Usage:  example-dnsquery-utf8 [name] [SRV|TXT]
 * Default: query TXT records for "google.com".
 *
 * Reference:
 * https://learn.microsoft.com/en-us/windows/win32/api/windns/nf-windns-dnsquery_utf8
 */

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <windns.h>

static void
print_bytes (const char *label, const char *s)
{
   size_t len = strlen (s);

   printf ("  %s: \"%s\"\n", label, s);
   printf ("    strlen=%zu bytes:", len);
   /* Dump up to 32 bytes so the encoding is unambiguous. */
   for (size_t i = 0; i < len && i < 32u; i++) {
      printf (" %02X", (unsigned char) s[i]);
   }
   if (len > 32u) {
      printf (" ...");
   }
   printf ("\n");
}

int
main (int argc, char *argv[])
{
   const char *name = (argc > 1) ? argv[1] : "google.com";
   const char *type_str = (argc > 2) ? argv[2] : "TXT";
   WORD wType;
   PDNS_RECORD results = NULL;
   PDNS_RECORD rec;
   DNS_STATUS status;

#ifdef UNICODE
   printf ("Built WITH UNICODE defined (PDNS_RECORD == PDNS_RECORDW).\n");
#else
   printf ("Built WITHOUT UNICODE defined (PDNS_RECORD == PDNS_RECORDA).\n");
#endif

   if (_stricmp (type_str, "SRV") == 0) {
      wType = DNS_TYPE_SRV;
   } else if (_stricmp (type_str, "TXT") == 0) {
      wType = DNS_TYPE_TEXT;
   } else {
      fprintf (stderr, "Unsupported record type '%s' (use SRV or TXT)\n", type_str);
      return 2;
   }

   printf ("Querying %s record(s) for \"%s\" via DnsQuery_UTF8...\n\n", type_str, name);

   status = DnsQuery_UTF8 (name, wType, DNS_QUERY_STANDARD, NULL /* pExtra */, &results, NULL /* pReserved */);
   if (status != ERROR_SUCCESS) {
      fprintf (stderr, "DnsQuery_UTF8 failed: status %ld (0x%08lX)\n", status, (unsigned long) status);
      return 1;
   }

   for (rec = results; rec != NULL; rec = rec->pNext) {
      if (rec->wType == DNS_TYPE_SRV && wType == DNS_TYPE_SRV) {
         /* pNameTarget is statically PWSTR under UNICODE, but DnsQuery_UTF8
          * fills it with UTF-8 char bytes; reinterpret accordingly. */
         const char *name_target = (const char *) rec->Data.SRV.pNameTarget;
         printf ("SRV record (priority=%u weight=%u port=%u):\n",
                 rec->Data.SRV.wPriority,
                 rec->Data.SRV.wWeight,
                 rec->Data.SRV.wPort);
         print_bytes ("pNameTarget", name_target);
      } else if (rec->wType == DNS_TYPE_TEXT && wType == DNS_TYPE_TEXT) {
         printf ("TXT record (%lu string(s)):\n", (unsigned long) rec->Data.TXT.dwStringCount);
         for (DWORD i = 0; i < rec->Data.TXT.dwStringCount; i++) {
            const char *str = (const char *) rec->Data.TXT.pStringArray[i];
            print_bytes ("string", str);
         }
      }
   }

   DnsRecordListFree (results, DnsFreeRecordList);
   return 0;
}

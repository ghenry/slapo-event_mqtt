/**
 * =====================================================================================
 *
 *          \file:  event_mqtt_utils.c
 *
 *    Description:  Given an existing dynamically allocated string, grow it to
 *                  cat a  printf style string into it. The goal being printing to a
 *                  string to create a larger string automatically.
 *
 *                  Returns -1 (EXIT_FAILURE) if fails or new size of string if OK
 *
 *       \version:  1.0
 *          \date:  18/02/17
 *       Revision:  none
 *       Compiler:  gcc
 *       Template:  cvim <http://www.vim.org/scripts/script.php?script_id=213>
 *     Formatting:  clang-format
 *
 *        \author:  Gavin Henry <ghenry@suretecsystems.com>
 *       \license:  GPLv2
 *     \copyright:  (c) 2017, Gavin Henry - Suretec Systems Ltd. T/A SureVoIP
 *   Organization:  Suretec Systems Ltd. T/A SureVoIP
 *
 * =====================================================================================
 */

#include "portable.h" /*  Needed before most OpenLDAP header files */

#include <ac/string.h>
#include <ac/ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/** Given an existing dynamically allocated string, grow it to
 *  cat a printf style string into it. The goal being printing to a
 *  string to create a larger string automatically.
 *
 *  \param ext_str External string via malloc/calloc etc.
 *  \param format  Noral printf style format
 *
 *  \returns -1 (EXIT_FAILURE) if fails or new size of string if OK
 */
int concatf(char **ext_str, char *format, ...) {
  va_list argp;
  va_start(argp, format);

  // p188-189 21st Century C - Test what we'll need for size
  char one_char[1];
  int len = vsnprintf(one_char, 1, format, argp);
  if (len < 1) {
    fprintf(stderr,
            "An encoding error has occurred. Leaving existing string alone.\n");
    return EXIT_FAILURE;
  }
  va_end(argp);

  int cur_size = strlen(*ext_str);
  int new_size = cur_size + len + 1; // Not forgetting '\0'

  *ext_str = realloc(*ext_str, new_size);
  if (!ext_str) {
    fprintf(stderr, "Couldn't allocate %i bytes.\n", new_size);
    return EXIT_FAILURE;
  }

  // I could of course create a second string and strcat it, but apparently
  // that's lazy and reckless (great read):
  // https://www.gnu.org/software/libc/manual/html_node/Concatenating-Strings.html#Concatenating-Strings

  va_start(argp, format);
  //  Move pointer to memory location of end of ext_str
  int new_len = vsnprintf(*ext_str + cur_size, len, format, argp);
  if (new_len < 1) {
    fprintf(stderr,
            "An encoding error has occurred. Leaving existing string alone.\n");
    return EXIT_FAILURE;
  }
  va_end(argp);

  return new_size; // How much was written
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/***************************************************************************
 *
 * polkit-grant-helper-shadow.c : setuid root shadow helper for PolicyKit
 *
 * Copyright (C) 2007 Piter PUNK, <piterpunk@slackware.com>
 *
 * Based on polkit-grant-helper-pam.c :
 *   Copyright (C) 2007 David Zeuthen, <david@fubar.dk>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <shadow.h>
#include <grp.h>
#include <pwd.h>

/* Development aid: define PGH_DEBUG to get debugging output. Do _NOT_
 * enable this in production builds; it may leak passwords and other
 * sensitive information.
 */
#undef PGH_DEBUG
/* #define PGH_DEBUG */

extern char *crypt ();
static int do_auth (const char *user_to_auth);

int main (int argc, char *argv[])
{
        char user_to_auth[256];

        /* clear the entire environment to avoid attacks with
         * libraries honoring environment variables */
        if (clearenv () != 0)
               goto error;
        /* set a minimal environment */
        setenv ("PATH", "/usr/sbin:/usr/bin:/sbin:/bin", 1);

        /* check that we are setuid root */
        if (geteuid () != 0) {
                fprintf (stderr, "polkit-grant-helper-shadow: needs to be setuid root\n");
                goto error;
        }

        openlog ("polkit-grant-helper-shadow", LOG_CONS | LOG_PID, LOG_AUTHPRIV);

        /* check for correct invocation */
        if (argc != 1) {
                syslog (LOG_NOTICE, "inappropriate use of helper, wrong number of arguments [uid=%d]", getuid ());
                fprintf (stderr, "polkit-grant-helper-shadow: wrong number of arguments. This incident has been logged.\n");
                goto error;
        }

        if (getuid () != 0) {
                /* check we're running with a non-tty stdin */
                if (isatty (STDIN_FILENO) != 0) {
                        syslog (LOG_NOTICE, "inappropriate use of helper, stdin is a tty [uid=%d]", getuid ());
                        fprintf (stderr, "polkit-grant-helper-shadow: inappropriate use of helper, stdin is a tty. This incident has been logged.\n");
                        goto error;
                }
        }

        /* get user to auth */
        if (fgets (user_to_auth, sizeof (user_to_auth), stdin) == NULL)
                goto error;
        if (strlen (user_to_auth) > 0 && user_to_auth[strlen (user_to_auth) - 1] == '\n')
                user_to_auth[strlen(user_to_auth) - 1] = '\0';

#ifdef PGH_DEBUG
        fprintf (stderr, "polkit-grant-helper-shadow: user to auth is '%s'.\n", user_to_auth);
#endif /* PGH_DEBUG */

        if(!do_auth (user_to_auth)) {
                syslog (LOG_NOTICE, "authentication failure [uid=%d] trying to authenticate '%s'", getuid (), user_to_auth);
                fprintf (stderr, "polkit-grant-helper-shadow: authentication failure. This incident has been logged.\n");
                goto error;
        }

#ifdef PGH_DEBUG
        fprintf (stderr, "polkit-grant-helper-shadow: successfully authenticated user '%s'.\n", user_to_auth);
#endif /* PGH_DEBUG */

        fprintf (stdout, "SUCCESS\n");
        fflush (stdout);
        return 0;

error:
        sleep (2); /* Discourage brute force attackers */
        fprintf (stdout, "FAILURE\n");
        fflush (stdout);
        return 1;
}
/* 
 * This is the shadow do_auth function. It receives
 * only the name of user (user_to_auth). Waits for
 * password in stdin and auth the user. It return success
 * if the user can be authenticated and unsuccess when
 * user can't be authenticated.
 */
int do_auth (const char *user_to_auth)
{
        struct spwd *shadow;
        char *password;
        char buf[256];

        if ((shadow = getspnam (user_to_auth)) == NULL)
                goto error;

        if (fgets (buf, sizeof (buf), stdin) == NULL)
                goto error;

        if (strlen (buf) > 0 &&
                buf[strlen (buf) - 1] == '\n')
                        buf[strlen (buf) - 1] = '\0';

        password = strdup (buf);

        if (strcmp (shadow->sp_pwdp, crypt (password, shadow->sp_pwdp)) != 0)
                goto error;

        return 1;

error:
        return 0;
}
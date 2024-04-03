/*--------------------------------------------------------------------*/
/*;  Copyright (C) 1995, 1999-2000                                    */
/*;  Associated Universities, Inc. Washington DC, USA.                */
/*;                                                                   */
/*;  This program is free software; you can redistribute it and/or    */
/*;  modify it under the terms of the GNU General Public License as   */
/*;  published by the Free Software Foundation; either version 2 of   */
/*;  the License, or (at your option) any later version.              */
/*;                                                                   */
/*;  This program is distributed in the hope that it will be useful,  */
/*;  but WITHOUT ANY WARRANTY; without even the implied warranty of   */
/*;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    */
/*;  GNU General Public License for more details.                     */
/*;                                                                   */
/*;  You should have received a copy of the GNU General Public        */
/*;  License along with this program; if not, write to the Free       */
/*;  Software Foundation, Inc., 675 Massachusetts Ave, Cambridge,     */
/*;  MA 02139, USA.                                                   */
/*;                                                                   */
/*;  Correspondence concerning AIPS should be addressed as follows:   */
/*;         Internet email: aipsmail@nrao.edu.                        */
/*;         Postal address: AIPS Project Office                       */
/*;                         National Radio Astronomy Observatory      */
/*;                         520 Edgemont Road                         */
/*;                         Charlottesville, VA 22903-2475 USA        */
/*--------------------------------------------------------------------*/
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif
#define Z_lock__
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>                      /* For System V 2.0+ read and */
                                        /* write locks.               */
#if __STDC__
   void main (int argc, char *argv[])
#else
   main(argc, argv)
      int argc;
      char *argv[];
#endif
/*--------------------------------------------------------------------*/
/* test program to check out fcntl() [posix] locking.  PPM 93.08.02   */
/*--------------------------------------------------------------------*/
{
  int fd;
  int ecode;
  char dum[80];
#ifdef _POSIX_SOURCE
   struct flock lock;
#endif

/*--------------------------------------------------------------------*/

  if (argc != 2) {
    printf("Usage: %s file\n\n", argv[0]);
    printf("Tells what pid is locking a file, if it is locked\n");
    exit(42);
  }
  /* get first arg */
  if ((fd = open (argv[1], O_RDWR, 0666)) == -1) {
    perror("Attempted open");
    exit(42);
  } else {
    printf("opened %s in rw-rw-rw- mode ok\n", argv[1]);
  }
#ifdef _POSIX_SOURCE
                                        /* Apply non-blocking lock.   */
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;
                                        /* Exclusive lock.            */
  lock.l_type = F_WRLCK;
                                        /* Shared lock                */
  /* lock.l_type = F_RDLCK; */

  printf ("(Using fcntl for locking)\n");
  if ( fcntl (fd, F_SETLK, &lock) == -1 ) {
    ecode = errno;
    printf ("Error code was %d\n", ecode);
    if ( errno == EACCES || errno == EAGAIN ) {
      perror ("Aha!  It's LOCKED!!!");
      if ( fcntl (fd, F_GETLK, &lock) == -1) {
	perror ("Can't get owner of lock");
	exit(42);
      }
      fprintf (stderr, "Owner of lock is pid %d\n", lock.l_pid);
      fprintf (stderr, "(this may be meaningless)\n");
    } else {                            /* Some other error           */
      perror ("fcntl F_SETLK");
    }

#else /* rest is NOT _POSIX_SOURCE */

  printf ("(Using flock for locking)\n");
  if (flock (fd, LOCK_EX | LOCK_NB) != 0) {
    if (errno == EWOULDBLOCK) {
      fprintf (stderr, "File is currently locked by someone else.\n");
      fprintf (stderr,
	       "Cannot tell who locked, used flock, not fcntl\n");
    } else {
      fprintf (stderr, "flock error %d\n", errno);
      perror ("cannot lock file");
    }

#endif /* _POSIX_SOURCE check */

  } else {
    printf ("File is now locked, press RETURN to unlock: ");
    fgets(dum, 10, stdin);
  }
}


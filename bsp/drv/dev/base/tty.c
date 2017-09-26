/*-
 * Copyright (c) 1982, 1986, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tty.c	8.13 (Berkeley) 1/9/95
 */

/* Modified for Prex by Kohsuke Ohtani. */

/*
 * tty.c - TTY device helper
 */

#include <driver.h>
#include <sys/signal.h>
#include <sys/termios.h>
#include <tty.h>
#include <pm.h>
#include <kd.h>

/* #define DEBUG_TTY 1 */

#ifdef DEBUG_TTY
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define	FREAD		0x0001
#define	FWRITE		0x0002

static void tty_output(int c, struct tty *tp);

/* default control characters */
static const cc_t	ttydefchars[NCCS] = TTYDEFCHARS;

#define is_ctrl(c)   ((c) < 32 || (c) == 0x7f)

/*
 * TTY queue operations
 */
#define ttyq_next(i)	(((i) + 1) & (TTYQ_SIZE - 1))
#define ttyq_prev(i)	(((i) - 1) & (TTYQ_SIZE - 1))
#define ttyq_full(q)	((q)->tq_count >= TTYQ_SIZE)
#define ttyq_empty(q)	((q)->tq_count == 0)

/*
 * Get a character from a queue.
 */
int
tty_getc(struct tty_queue *tq)
{
	char c;
	int s;

	s = splhigh();
	if (ttyq_empty(tq)) {
		splx(s);
		return -1;
	}
	c = tq->tq_buf[tq->tq_head];
	tq->tq_head = ttyq_next(tq->tq_head);
	tq->tq_count--;
	splx(s);
	return (int)c;
}

/*
 * Put a character into a queue.
 */
static void
tty_putc(int c, struct tty_queue *tq)
{
	int s;

	s = splhigh();
	if (ttyq_full(tq)) {
		splx(s);
		return;
	}
	tq->tq_buf[tq->tq_tail] = (char)(c & 0xff);
	tq->tq_tail = ttyq_next(tq->tq_tail);
	tq->tq_count++;
	splx(s);
}

/*
 * Remove the last character in a queue and return it.
 */
static int
tty_unputc(struct tty_queue *tq)
{
	char c;
	int s;

	if (ttyq_empty(tq))
		return -1;
	s = splhigh();
	tq->tq_tail = ttyq_prev(tq->tq_tail);
	c = tq->tq_buf[tq->tq_tail];
	tq->tq_count--;
	splx(s);
	return (int)c;
}

/*
 * Put the chars in the from queue on the end of the to queue.
 */
static void
tty_catq(struct tty_queue *from, struct tty_queue *to)
{
	int c;

	while ((c = tty_getc(from)) != -1)
		tty_putc(c, to);
}

/*
 * Rubout one character from the rawq of tp
 */
static void
tty_rubout(int c, struct tty *tp)
{

	if (!(tp->t_lflag & ECHO))
		return;
	if (tp->t_lflag & ECHOE) {
		tty_output('\b', tp);
		tty_output(' ', tp);
		tty_output('\b', tp);
	} else
		tty_output(tp->t_cc[VERASE], tp);
}

/*
 * Echo char
 */
static void
tty_echo(int c, struct tty *tp)
{

	if (!(tp->t_lflag & ECHO)) {
		if (c == '\n' && (tp->t_lflag & ECHONL))
			tty_output('\n', tp);
		return;
	}
	if (is_ctrl(c) && c != '\n' && c != '\t' && c != '\b') {
		tty_output('^', tp);
		tty_output(c + 'A' - 1, tp);
	} else
		tty_output(c, tp);
}

/*
 * Start output.
 */
static void
tty_start(struct tty *tp)
{

	DPRINTF(("tty_start\n"));

	if (tp->t_state & (TS_TTSTOP|TS_BUSY))
		return;
	if (tp->t_oproc)
		(*tp->t_oproc)(tp);
}

/*
 * Flush tty read and/or write queues, notifying anyone waiting.
 */
static void
tty_flush(struct tty *tp, int rw)
{

	DPRINTF(("tty_flush rw=%d\n", rw));

	if (rw & FREAD) {
		while (tty_getc(&tp->t_canq) != -1)
			;
		while (tty_getc(&tp->t_rawq) != -1)
			;
		sched_wakeup(&tp->t_input);
	}
	if (rw & FWRITE) {
		tp->t_state &= ~TS_TTSTOP;
		tty_start(tp);
	}
}

/*
 * Output is completed.
 */
void
tty_done(struct tty *tp)
{

	if (tp->t_outq.tq_count == 0)
		tp->t_state &= ~TS_BUSY;
	if (tp->t_state & TS_ASLEEP) {
		tp->t_state &= ~TS_ASLEEP;
		sched_wakeup(&tp->t_output);
	}
}

/*
 * Wait for output to drain.
 */
static void
tty_wait(struct tty *tp)
{

	/*	DPRINTF(("tty_wait\n")); */

	if ((tp->t_outq.tq_count > 0) && tp->t_oproc) {
		tp->t_state |= TS_BUSY;
		while (1) {
			(*tp->t_oproc)(tp);
			if ((tp->t_state & TS_BUSY) == 0)
				break;
			tp->t_state |= TS_ASLEEP;
			sched_sleep(&tp->t_output);
		}
	}
}

/*
 * Send TTY signal at DPC level.
 */
static void
tty_signal(void *arg)
{
	struct tty *tp = arg;

	DPRINTF(("tty_signal sig=%d\n", tp->t_signo));
	exception_post(tp->t_sigtask, tp->t_signo);
}

/*
 * Process input of a single character received on a tty.
 * echo if required.
 * This may be called with interrupt level.
 */
void
tty_input(int c, struct tty *tp)
{
	unsigned char *cc;
	tcflag_t iflag, lflag;
	int sig = -1;

	DPRINTF(("tty_input: %d\n", c));

	/* Reload power management timer */
	pm_notify(PME_USER_ACTIVITY);

	lflag = tp->t_lflag;
	iflag = tp->t_iflag;
	cc = tp->t_cc;

#if defined(DEBUG) && defined(CONFIG_KD)
	if (c == cc[VDDB]) {
		kd_enter();
		tty_flush(tp, FREAD | FWRITE);
		goto endcase;
	}
#endif /* !CONFIG_KD*/

	/* IGNCR, ICRNL, INLCR */
	if (c == '\r') {
		if (iflag & IGNCR)
			goto endcase;
		else if (iflag & ICRNL)
			c = '\n';
	} else if (c == '\n' && (iflag & INLCR))
		c = '\r';

	if (iflag & IXON) {
		/* stop (^S) */
		if (c == cc[VSTOP]) {
			if (!(tp->t_state & TS_TTSTOP)) {
				tp->t_state |= TS_TTSTOP;
				return;
			}
			if (c != cc[VSTART])
				return;
			/* if VSTART == VSTOP then toggle */
			goto endcase;
		}
		/* start (^Q) */
		if (c == cc[VSTART])
			goto restartoutput;
	}
	if (lflag & ICANON) {
		/* erase (^H / ^?) or backspace */
		if (c == cc[VERASE] || c == '\b') {
			if (!ttyq_empty(&tp->t_rawq))
				tty_rubout(tty_unputc(&tp->t_rawq), tp);
			goto endcase;
		}
		/* kill (^U) */
		if (c == cc[VKILL]) {
			while (!ttyq_empty(&tp->t_rawq))
				tty_rubout(tty_unputc(&tp->t_rawq), tp);
			goto endcase;
		}
	}
	if (lflag & ISIG) {
		/* quit (^C) */
		if (c == cc[VINTR] || c == cc[VQUIT]) {
			if (!(lflag & NOFLSH)) {
				tp->t_state |= TS_ISIG;
				tty_flush(tp, FREAD | FWRITE);
			}
			tty_echo(c, tp);
			sig = (c == cc[VINTR]) ? SIGINT : SIGQUIT;
			goto endcase;
		}
		/* suspend (^Z) */
		if (c == cc[VSUSP]) {
			if (!(lflag & NOFLSH)) {
				tp->t_state |= TS_ISIG;
				tty_flush(tp, FREAD | FWRITE);
			}
			tty_echo(c, tp);
			sig = SIGTSTP;
			goto endcase;
		}
	}

	/*
	 * Check for input buffer overflow
	 */
	if (ttyq_full(&tp->t_rawq)) {
		tty_flush(tp, FREAD | FWRITE);
		goto endcase;
	}
	tty_putc(c, &tp->t_rawq);

	if (lflag & ICANON) {
		/*EDIT if (c == '\n' || c == cc[VEOF] || c == cc[VEOL]) */{
			tty_catq(&tp->t_rawq, &tp->t_canq);
			sched_wakeup(&tp->t_input);
		}
	} else
		sched_wakeup(&tp->t_input);

/*EDIT	if (lflag & ECHO)
		tty_echo(c, tp);*/
 endcase:
	/*
	 * IXANY means allow any character to restart output.
	 */
	if ((tp->t_state & TS_TTSTOP) && (iflag & IXANY) == 0 &&
	    cc[VSTART] != cc[VSTOP])
		return;
 restartoutput:
	tp->t_state &= ~TS_TTSTOP;

	if (sig != -1) {
		if (tp->t_sigtask) {
			tp->t_signo = sig;
			sched_dpc(&tp->t_dpc, &tty_signal, tp);
		}
	}
	tty_start(tp);
}

/*
 * Output a single character on a tty, doing output processing
 * as needed (expanding tabs, newline processing, etc.).
 */
static void
tty_output(int c, struct tty *tp)
{
	int i, col;

	if ((tp->t_lflag & ICANON) == 0) {
		tty_putc(c, &tp->t_outq);
		return;
	}
	/* Expand tab */
	if (c == '\t' && (tp->t_oflag & OXTABS)) {
		i = 8 - (tp->t_column & 7);
		tp->t_column += i;
		do
			tty_putc(' ', &tp->t_outq);
		while (--i > 0);
		return;
	}
	/* Translate newline into "\r\n" */
	if (c == '\n' && (tp->t_oflag & ONLCR))
		tty_putc('\r', &tp->t_outq);

	tty_putc(c, &tp->t_outq);

	col = tp->t_column;
	switch (c) {
	case '\b':	/* back space */
		if (col > 0)
			--col;
		break;
	case '\t':	/* tab */
		col = (col + 8) & ~7;
		break;
	case '\n':	/* newline */
		col = 0;
		break;
	case '\r':	/* return */
		col = 0;
		break;
	default:
		if (!is_ctrl(c))
		    ++col;
		break;
	}
	tp->t_column = col;
	return;
}

/*
 * Process a read call on a tty device.
 */
int
tty_read(struct tty *tp, char *buf, size_t *nbyte)
{
	unsigned char *cc;
	struct tty_queue *qp;
	int rc, tmp;
	u_char c;
	size_t count = 0;
	tcflag_t lflag;

	DPRINTF(("tty_read\n"));

	lflag = tp->t_lflag;
	cc = tp->t_cc;
	qp = (lflag & ICANON) ? &tp->t_canq : &tp->t_rawq;

	/* If there is no input, wait it */
	while (ttyq_empty(qp)) {
		rc = sched_sleep(&tp->t_input);
		if (rc == SLP_INTR || tp->t_state & TS_ISIG) {
			tp->t_state &= ~TS_ISIG;
			return EINTR;
		}
	}
	while (count < *nbyte) {
		if ((tmp = tty_getc(qp)) == -1)
			break;
		c = (u_char)tmp;
		if (c == cc[VEOF] && (lflag & ICANON))
			break;
		count++;
		if (copyout(&c, buf, 1))
			return EFAULT;
		if ((lflag & ICANON) && (c == '\n' || c == cc[VEOL]))
			break;
		buf++;
	}
	*nbyte = count;
	return 0;
}

/*
 * Process a write call on a tty device.
 */
int
tty_write(struct tty *tp, char *buf, size_t *nbyte)
{
	size_t remain, count = 0;
	u_char c;

	DPRINTF(("tty_write\n"));

	remain = *nbyte;
	while (remain > 0) {
		if (tp->t_outq.tq_count > TTYQ_HIWAT) {
			tty_start(tp);
			if (tp->t_outq.tq_count <= TTYQ_HIWAT)
				continue;
			tp->t_state |= TS_ASLEEP;
			sched_sleep(&tp->t_output);
			continue;
		}
		if (copyin(buf, &c, 1))
			return EFAULT;
		tty_output((int)c, tp);
		buf++;
		remain--;
		count++;
	}
	tty_start(tp);
	*nbyte = count;
	return 0;
}

/*
 * Ioctls for all tty devices.
 */
int
tty_ioctl(struct tty *tp, u_long cmd, void *data)
{
	int flags;
	struct tty_queue *qp;

	switch (cmd) {
	case TIOCGETA:
		if (copyout(&tp->t_termios, data,
				 sizeof(struct termios)))
			return EFAULT;
		break;
	case TIOCSETAW:
	case TIOCSETAF:
		tty_wait(tp);
		if (cmd == TIOCSETAF)
			tty_flush(tp, FREAD);
		/* FALLTHROUGH */
	case TIOCSETA:
		if (copyin(data, &tp->t_termios,
			   sizeof(struct termios)))
			return EFAULT;
		break;
	case TIOCSPGRP:			/* set pgrp of tty */
		if (copyin(data, &tp->t_pgid, sizeof(pid_t)))
			return EFAULT;
		break;
	case TIOCGPGRP:
		if (copyout(&tp->t_pgid, data, sizeof(pid_t)))
			return EFAULT;
		break;
	case TIOCFLUSH:
		if (copyin(data, &flags, sizeof(flags)))
			return EFAULT;
		if (flags == 0)
			flags = FREAD | FWRITE;
		else
			flags &= FREAD | FWRITE;
		tty_flush(tp, flags);
		break;
	case TIOCSTART:
		if (tp->t_state & TS_TTSTOP) {
			tp->t_state &= ~TS_TTSTOP;
			tty_start(tp);
		}
		break;
	case TIOCSTOP:
		if (!(tp->t_state & TS_TTSTOP)) {
			tp->t_state |= TS_TTSTOP;
		}
		break;
	case TIOCGWINSZ:
		if (copyout(&tp->t_winsize, data,
			    sizeof(struct winsize)))
			return EFAULT;
		break;
	case TIOCSWINSZ:
		if (copyin(data, &tp->t_winsize,
			   sizeof(struct winsize)))
			return EFAULT;
		break;
	case TIOCSETSIGT:	/* Prex */
		if (copyin(data, &tp->t_sigtask, sizeof(task_t)))
			return EFAULT;
		break;
	case TIOCINQ:
		qp = (tp->t_lflag & ICANON) ? &tp->t_canq : &tp->t_rawq;
		if (copyout(&qp->tq_count, data, sizeof(int)))
			return EFAULT;
		break;
	case TIOCOUTQ:
		if (copyout(&tp->t_outq.tq_count, data, sizeof(int)))
			return EFAULT;
		break;
	}
	return 0;
}

/*
 * Attach a tty to the tty list.
 */
void
tty_attach(struct tty *tp)
{
	struct bootinfo *bi;

	/* Initialize tty */
	memset(tp, 0, sizeof(struct tty));
	memcpy(&tp->t_termios.c_cc, ttydefchars, sizeof(ttydefchars));

	event_init(&tp->t_input, "TTY input");
	event_init(&tp->t_output, "TTY output");

	tp->t_iflag = TTYDEF_IFLAG;
	tp->t_oflag = TTYDEF_OFLAG;
	tp->t_cflag = TTYDEF_CFLAG;
	tp->t_lflag = TTYDEF_LFLAG;
	tp->t_ispeed = TTYDEF_SPEED;
	tp->t_ospeed = TTYDEF_SPEED;

	machine_bootinfo(&bi);
	tp->t_winsize.ws_row = (u_short)bi->video.text_y;
	tp->t_winsize.ws_col = (u_short)bi->video.text_x;
}

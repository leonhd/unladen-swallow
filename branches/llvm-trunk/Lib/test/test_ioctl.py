import unittest
from test.test_support import TestSkipped, run_unittest
import os, struct
try:
    import _llvm
except ImportError:
    _llvm = None

try:
    import fcntl, termios
except ImportError:
    raise TestSkipped("No fcntl or termios module")
if not hasattr(termios,'TIOCGPGRP'):
    raise TestSkipped("termios module doesn't have TIOCGPGRP")

try:
    tty = open("/dev/tty", "r")
    tty.close()
except IOError:
    raise TestSkipped("Unable to open /dev/tty")

try:
    import pty
except ImportError:
    pty = None

class IoctlTests(unittest.TestCase):

    def setUp(self):
        # TODO(rnk): This test doesn't pass with the background thread under
        # -Xjit=always, so we disable the JIT for the duration of the tests.
        if _llvm:
            self.orig_jit_control = _llvm.get_jit_control()
            _llvm.set_jit_control("never")

    def tearDown(self):
        if _llvm:
            _llvm.set_jit_control(self.orig_jit_control)

    def test_ioctl(self):
        # If this process has been put into the background, TIOCGPGRP returns
        # the session ID instead of the process group id.
        ids = (os.getpgrp(), os.getsid(0))
        tty = open("/dev/tty", "r")
        r = fcntl.ioctl(tty, termios.TIOCGPGRP, "    ")
        rpgrp = struct.unpack("i", r)[0]
        self.assert_(rpgrp in ids, "%s not in %s" % (rpgrp, ids))

    def test_ioctl_mutate(self):
        import array
        buf = array.array('i', [0])
        ids = (os.getpgrp(), os.getsid(0))
        tty = open("/dev/tty", "r")
        r = fcntl.ioctl(tty, termios.TIOCGPGRP, buf, 1)
        rpgrp = buf[0]
        self.assertEquals(r, 0)
        self.assert_(rpgrp in ids, "%s not in %s" % (rpgrp, ids))

    def test_ioctl_signed_unsigned_code_param(self):
        if not pty:
            raise TestSkipped('pty module required')
        mfd, sfd = pty.openpty()
        try:
            if termios.TIOCSWINSZ < 0:
                set_winsz_opcode_maybe_neg = termios.TIOCSWINSZ
                set_winsz_opcode_pos = termios.TIOCSWINSZ & 0xffffffffL
            else:
                set_winsz_opcode_pos = termios.TIOCSWINSZ
                set_winsz_opcode_maybe_neg, = struct.unpack("i",
                        struct.pack("I", termios.TIOCSWINSZ))

            our_winsz = struct.pack("HHHH",80,25,0,0)
            # test both with a positive and potentially negative ioctl code
            new_winsz = fcntl.ioctl(mfd, set_winsz_opcode_pos, our_winsz)
            new_winsz = fcntl.ioctl(mfd, set_winsz_opcode_maybe_neg, our_winsz)
        finally:
            os.close(mfd)
            os.close(sfd)

def test_main():
    run_unittest(IoctlTests)

if __name__ == "__main__":
    test_main()

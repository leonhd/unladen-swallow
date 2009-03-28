"""
i18n.py - internationalization support for mercurial

Copyright 2005, 2006 Matt Mackall <mpm@selenic.com>

This software may be used and distributed according to the terms
of the GNU General Public License, incorporated herein by reference.
"""

import gettext, sys, os

# modelled after templater.templatepath:
if hasattr(sys, 'frozen'):
    module = sys.executable
else:
    module = __file__

base = os.path.dirname(module)
for dir in ('.', '..'):
    localedir = os.path.normpath(os.path.join(base, dir, 'locale'))
    if os.path.isdir(localedir):
        break

t = gettext.translation('hg', localedir, fallback=True)

def gettext(message):
    """Translate message.

    The message is looked up in the catalog to get a Unicode string,
    which is encoded in the local encoding before being returned.

    Important: message is restricted to characters in the encoding
    given by sys.getdefaultencoding() which is most likely 'ascii'.
    """
    # If message is None, t.ugettext will return u'None' as the
    # translation whereas our callers expect us to return None.
    if message is None:
        return message

    # We cannot just run the text through util.tolocal since that
    # leads to infinite recursion when util._encoding is invalid.
    try:
        u = t.ugettext(message)
        return u.encode(util._encoding, "replace")
    except LookupError:
        return message

_ = gettext

# Moved after _ because of circular import.
import util
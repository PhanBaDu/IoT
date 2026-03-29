# -*- coding: utf-8 -*-
import re

path_tex = r'c:\Users\Du\Desktop\ProjectIoT\bao-cao.tex'
with open(path_tex, 'r', encoding='utf-8') as f:
    content = f.read()

print('Original length:', len(content))

# Step 1: Fix inline IfFileExists in prose line
# In the file, the line looks like:
# ... $\rightarrow$ \IfFileExists{appendix-iot-nhom6-nocomment.ino}{%
#   \lstinputlisting[
marker1 = '\IfFileExists{appendix-iot-nhom6-nocomment.ino}{%\n  \lstinputlisting['
repl1 = '\file{appendix-iot-nhom6-nocomment.ino}.'
if marker1 in content:
    content = content.replace(marker1, repl1, 1)
    print('OK: fixed inline IfFileExists')
else:
    print('NOTE: inline IfFileExists not found')

# Step 2: Remove the nested lstinputlisting inside the old IfFileExists block
# The block goes from "  \lstinputlisting[" to "  \end{mdframed}\n}\n"
# Find the start of the nested block
marker2 = '  \lstinputlisting['
end2 = '  \end{mdframed}\n}\n'
if marker2 in content:
    start = content.find(marker2)
    end = content.find(end2, start)
    if end >= 0:
        remove_end = end + len(end2)
        content = content[:start] + content[remove_end:]
        print('OK: removed nested lstinputlisting')
    else:
        print('NOTE: end of nested block not found')
else:
    print('NOTE: nested lstinputlisting not found')

# Step 3: Remove duplicate lstlisting block (has inputpath=, literate=)
# This block starts with "\lstinputlisting[" and ends with "appendix-iot-nhom6-nocomment.ino}\n"
marker3 = '\lstinputlisting['
end3_marker = 'appendix-iot-nhom6-nocomment.ino}'
if 'inputpath=.,' in content:
    pos3 = content.find(marker3)
    end3 = content.find(end3_marker, pos3)
    if end3 >= 0:
        remove_end3 = end3 + len(end3_marker) + 1  # +1 for newline
        content = content[:pos3] + content[remove_end3:]
        print('OK: removed duplicate lstlisting')
    else:
        print('NOTE: end of duplicate block not found')
else:
    print('NOTE: inputpath not found in content')

# Step 4: Insert clean \IfFileExists before "% PHU LUC B — Node-RED"
phub = '\n% =============================================================================\n% PHU LUC B \u2014 Node-RED Flow'
clean = '''
\IfFileExists{appendix-iot-nhom6-nocomment.ino}{%
  \lstinputlisting[
    style=appendixcode,
    caption={Firmware ESP8266 (\u0111\u00e3 g\u1ecf ch\u00fa th\u00edch, 487 d\u00f2ng)},
    label={lst:appendix-arduino}
  ]{appendix-iot-nhom6-nocomment.ino}%
}{%
  \begin{mdframed}[backgroundcolor=yellow!20, linecolor=yellow!60]
    \textbf{L\u1ed7i:} Kh\u00f4ng t\u00ecm th\u1ea5y \file{appendix-iot-nhom6-nocomment.ino}. Ch\u1ea1y l\u1ec7nh sau \u0111\u1ec3 t\u1ea1o file (y\u00eau c\u1ea7u Python 3):\\[0.4em]
    \code{python tools/strip_ino_comments.py}\\[0.4em]
    Sau \u0111\u00f3 \u0111\u1eb7t \file{appendix-iot-nhom6-nocomment.ino} c\u00f9ng th\u01b0 m\u1ee5c v\u1edbi \file{bao-cao.tex}.
  \end{mdframed}
}
'''
if phub in content:
    content = content.replace(phub, clean + phub, 1)
    print('OK: inserted clean IfFileExists')
else:
    # Try without PHU LUC B unicode
    phub2 = '\n% =============================================================================\n% PH'
    print('Searching for PHU LUC B marker...')
    idx = content.find('% PHU LUC B')
    if idx >= 0:
        print('Found at idx:', idx, 'Context:', repr(content[idx-50:idx+80]))
    else:
        print('PHU LUC B not found')

print('Final length:', len(content))
with open(path_tex, 'w', encoding='utf-8') as f:
    f.write(content)
print('Done writing bao-cao.tex')

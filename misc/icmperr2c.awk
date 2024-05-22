# icmperr2c.awk - script to convert icmp-errors.txt to C source
# code mapping error numbers to text.
# See https://learn.microsoft.com/en-us/windows/win32/api/ipexport/ns-ipexport-icmp_echo_reply32
# This is necessary because FormatMessage() does not work for ICMP errors;
# arrgh.
# Mark Riordan 22-MAY-2024
#
# Input consists of groups of 3 lines like this:
# IP_BUF_TOO_SMALL
# 11001
# The reply buffer was too small.
#
# Usage: awk95 -f icmperr2c.awk icmp-errors.txt > icmp-errors.c
{
    if (NR % 3 == 1) {
        errident = $1
    } else if (NR % 3 == 2) {
        errnum = $1
    } else {
        errtext = $0
        print "{" errnum, ", \"" errident "\", \"" errtext "\"},"
    }
}

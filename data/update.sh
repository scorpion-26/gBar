#/bin/sh

# This script is not used by default. It is a human-readable version of the default Arch-applicable
# package command with explanations

updates="$(checkupdates)";
exitCode=$?;

if [ $exitCode -eq 127 ] ; then
    # checkupdates wasn't found.
    # Forward the error to gBar, so gBar can shut down the widget
    # This is done, so we don't bother non-Arch systems with update checking
    exit 127;
fi

if [ $exitCode -eq 2 ] ; then
    # Zero packages out-of-date. We need to handle this case, since 'echo "$updates" | wc -l' would return 1
    echo "0" && exit 0
fi

# We need the extra newline (-n option omitted), since 'echo -n $"updates" | wc -l' is off by one,
# since 'echo -n $"updates"' has a \0 at the end
echo "$updates" | wc -l

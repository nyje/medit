if [ -z "$2" ]; then
    echo usage: "$0 <var_name> <file_name>"
    exit 1
fi

VARNAME="$1"
INPUT="$2"

echo "/* -*- C -*- */"
echo "static const char $VARNAME [] = \"\""
sed 's/"/\\"/g' "$INPUT" | sed 's/^\(.*\)$/"\1\\n"/' || exit $?
echo ";"

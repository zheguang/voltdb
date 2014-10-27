#!/usr/bin/env bash

declare USE_ADHOC_PROC=no
declare PREPOPULATE_DATA=no

if [ -n "$(which voltdb 2> /dev/null)" ]; then
    VOLTDB_BASE=$(dirname $(dirname $(which voltdb)))
else
    echo "The VoltDB scripts are not in your PATH."
    exit 1
fi

function _push() {
    pushd scenarios/$1 > /dev/null || exit 1
}

function _pop() {
    popd > /dev/null || exit 1
}

function _compile() {
    echo "Compiling Java..."
    mkdir -p obj
    local CP=$CLASSPATH:$({ \
        \ls -1 $VOLTDB_BASE/voltdb/voltdb-*.jar; \
        \ls -1 $VOLTDB_BASE/lib/*.jar; \
        \ls -1 $VOLTDB_BASE/lib/extension/*.jar; \
    } 2> /dev/null | paste -sd ':' - )
    javac -target 1.7 -source 1.7 -classpath $CP -d obj src/voter/*.java src/voter/procedures/*.java
    if [ $? -ne 0 ]; then
        echo "Java compilation failed."
        exit 1
    fi
}

function _catalog() {
    echo "Compiling catalog..."
    voltdb compile --classpath obj -o voter.jar ../../ddl.sql > catalog.out
    if [ $? -ne 0 ]; then
        cat catalog.out
        exit 1
    fi
}

function _atexit() {
    if [ $? -eq 0 ]; then
        echo SUCCESS
    else
        echo FAILURE
    fi
    voltdb stop
}

function _scenario() {
    echo "Context[$1]: BEGIN"
    pushd scenarios/$1 > /dev/null || exit 1
    trap "_pop; echo \"Context[$1]: END\"; trap RETURN" RETURN
    shift
    "$@"
}

function _server() {
    echo "Starting server..."
    voltdb create -B -d ../../deployment.xml -l $VOLTDB_BASE/voltdb/license.xml voter.jar || exit 1
    trap "_atexit" EXIT
    while ! grep -q 'Server completed initialization.' ~/.voltdb_server/localhost_3021.out; do
        echo "Waiting for server to initialize..."
        sleep 5
    done
}

function _client() {
    echo "Running client..."
    local CP=$CLASSPATH:$({ \
        \ls -1 $VOLTDB_BASE/voltdb/voltdbclient-*.jar; \
        \ls -1 $VOLTDB_BASE/lib/commons-cli-1.2.jar; \
    } 2> /dev/null | paste -sd ':' - )
    java -classpath obj:$CP:obj -Dlog4j.configuration=file://$VOLTDB_BASE/voltdb/log4j.xml voter.TestClient || exit 1
}

function _ddl() {
    echo "Executing DDL..."
    echo "$@"
    if [ "$USE_ADHOC_PROC" = "yes" ]; then
        echo "exec @AdHoc '$@'" | sqlcmd > /dev/null || exit 1
    else
        echo "$@" | sqlcmd > /dev/null || exit 1
    fi
}

function _sql() {
    echo "Executing SQL..."
    echo "$@"
    if [ "$USE_ADHOC_PROC" = "yes" ]; then
        echo "exec @AdHoc '$@'" | sqlcmd || exit 1
    else
        echo "$@" | sqlcmd > /dev/null || exit 1
    fi
}

function _jar() {
    echo "Creating jar $1 from $2.class..."
    pushd obj > /dev/null || exit 1
    jar cf ../$1 voter/procedures/$2.class || exit 1
    popd > /dev/null || exit 1
}

function test_base() {
    _scenario base _client
}

# SUCCESS: empty or non-empty
function test_add-column() {
    _ddl "alter table votes add column ssn varchar(20);"
    _scenario base _client
}

# SUCCESS: empty or non-empty
# Broken in sqlcmd, works with exec @AdHoc
function test_drop-column() {
    _ddl "alter table votes add column ssn varchar(20);"
    _sql "select * from votes limit 0" | grep PHONE_NUMBER
    _scenario base _client
    _ddl "alter table votes drop column ssn;"
    _sql "select * from votes limit 0" | grep PHONE_NUMBER
    _scenario base _client
}

# FAILURE: empty or non-empty
# Error: "Column SSN has no default and is not nullable."
function test_add-not-null() {
    _ddl "alter table votes add column ssn varchar(20) not null;"
    _scenario base _client
}

# SUCCESS: empty or non-empty
function test_add-with-default() {
    _ddl "alter table votes add column ssn varchar(20) default 'SSN' not null;"
    _scenario base _client
}

# SUCCESS: empty or non-empty
function test_column-and-proc() {
    _scenario column-and-proc _compile
    _scenario column-and-proc _jar update.jar Vote
    _scenario column-and-proc _ddl "\
alter table votes add column ssn varchar(20); \
exec @UpdateClasses 'update.jar' '';"
    _scenario column-and-proc _client
}

# FAILURE: empty or non-empty
# Unexpected exception applying DDL statements to  original catalog: DDL Error: "unexpected token: LIMIT"
function test_constraint() {
    _ddl "\
alter table votes add column ssn varchar(20) default 'SSN' not null; \
alter table votes add constraint CON_LIMIT_SSN limit partition rows 100 (ssn);"
}

function test_default() {
    _ddl "alter table votes add column ssn varchar(20);"
    _ddl "alter table votes alter ssn set default 'XXX-XX-XXXX';"
    _sql "insert into votes (phone_number, state, contestant_number) values ('1111111','WA',1111111)"
    _sql "select ssn from votes where phone_number = 1111111" | grep -q XXX-XX-XXXX || exit 1
}

function _start() {
    _compile
    _catalog
    _server
}

function _check_function() {
    if ! declare -Ff "$1" >/dev/null; then
        echo "* $2 not found *"
        _usage
    fi
}

function run_clean() {
    local D
    for D in . scenarios/*; do
        rm -rf $D/obj $D/debugoutput $D/*.jar $D/voltdbroot $D/log $D/*.txt \
               $D/statement-plans $D/catalog.out $D/catalog-report.html
    done
}

# argument 1: test name
function run_test() {
    if [ -z "$1" ]; then
        echo "* No test specified *"
        _usage
    fi
    local FUNCTION=test_$1
    _check_function $FUNCTION "test $1"
    shift
    run_clean
    _scenario base _start
    if [ "$PREPOPULATE_DATA" = "yes" ]; then
        _scenario base _client
        shift
    fi
    $FUNCTION "$@"
}

function _usage() {
    local NAME=$(basename $0)
    echo "
Usage:
  $NAME [OPTIONS] test NAME
  $NAME clean

  OPTIONS
    -p pre-populate data
    -s use @AdHoc system procedure

  NAME
    base
    add-column
    add-not-null
    add-with-default
    column-and-proc
    constraint
    default
    drop-column
"
    exit 1
}

args=$(getopt "ps" $*)
if [ $? != 0 ]; then
    _usage
fi
set -- $args
for ARG; do
   case "$ARG"
   in
   -p)
       export PREPOPULATE_DATA=yes
       shift;;
   -s)
       export USE_ADHOC_PROC=yes
       shift;;
   --)
       shift; break;;
   esac
done

declare FUNCTION="run_$1"
_check_function $FUNCTION "sub-command $1"
shift

$FUNCTION "$@"

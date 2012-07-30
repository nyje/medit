if [ -z "$OSTYPE" -o "$OSTYPE" != "cygwin" ]; then
  export mgwsystem=linux
else
  export mgwsystem=cygwin
fi
if [ $mgwsystem = linux ]; then
  export mgwlinux=true
  export mgwcygwin=false
else
  export mgwlinux=false
  export mgwcygwin=true
fi

export mgwbuildroot=$HOME/projects/gtk-win-build
export mgwpythondotver=2.7

export mgwpythonver=`echo $mgwpythondotver | sed 's/[.]//'`

if $mgwlinux; then
  export mgwpythoninstdir=$HOME/.wine/drive_c/Python$mgwpythonver
  if [ -d $HOME/.wine/drive_c/windows/syswow64 ]; then
    export mgwpythonsystem32dir=$HOME/.wine/drive_c/windows/syswow64
  else
    export mgwpythonsystem32dir=$HOME/.wine/drive_c/windows/system32
  fi
else
  export mgwpythoninstdir=/cygdrive/c/Tools/Python$mgwpythonver
  export mgwpythonsystem32dir=/cygdrive/c/Windows/SysWOW64
fi

export mgwdir_s=$mgwdir
export mgwdir=`cd $mgwdir && pwd`

if [ -z "$mgwconfig" ]; then
  export mgwconfig=release
fi

case "$1" in
  --debug)
    mgwconfig=debug
    shift
    ;;
  --release)
    mgwconfig=release
    shift
    ;;
  --reldbg)
    mgwconfig=reldbg
    shift
    ;;
esac

export mgwjhbuildsrcdir=$mgwbuildroot/jhbuild
export mgwjhbuilddir=$mgwbuildroot/jhbuild-bin

export mgwdestdir=$mgwbuildroot/bdist-$mgwconfig

export mgwbuilddir=$mgwbuildroot/$mgwconfig
export mgwbuilddir_s=$mgwbuildroot/$mgwconfig
export mgwsourcedir=$mgwbuilddir/source
export mgwtargetdir=$mgwbuilddir/target
export mgwsourcedir_s=$mgwbuilddir_s/source
export mgwtargetdir_s=$mgwbuilddir_s/target
export mgwdistdir=$mgwbuildroot/dist-$mgwconfig

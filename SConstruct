# -----------------------------------------------------------------------------

import sys
import os
import platform
import subprocess
import fnmatch

# -----------------------------------------------------------------------------

Help("""
Usage:
  % scons
      does a release build, auto-detects your system

  Flags you can pass when calling 'scons' :
      config=[debug/release]
          specify build type
      jack=[0/1]
          whether to enable the Jack MIDI driver
      compiler_arch=[32bit/64bit]
          specify whether the compiler will build as 32 bits or 64 bits
          (does not add flags to cross-compile, only selects lib dirs)
      renderer=[opengl/wxwidgets]
          choose whether to use the OpenGL renderer or the software
          renderer based on wxWidgets
      CXXFLAGS="custom build flags"
          To add other flags to pass when compiling
      LDFLAGS="custom link flags"
          To add other flags to pass when linking

  Furthermore, the CXX environment variable is read if it exists, allowing
  you to choose which g++ executable you wish to use.
  The PATH environment variable is also considered.

  % scons install
      Installs Aria, auto-detects system (run as root if necessary)

  Flags you can pass when calling 'scons install':
      prefix=[/opt/local or something else]
          install to a different prefix than default /usr/local

  % scons uninstall
      Uninstalls Aria, takes same flags as 'scons install'.
      If you specified a custom install prefix, you need to specify it again.
      * Not available on mac OS X, just drag the generated app to the trash.

  Use scons -H for help about scons itself, and its terminal flags.
""")

# -----------------------------------------------------------------------------

class RecursiveGlob:
    def __init__(self, directory, pattern="*"):
        self.dir_stack = [directory]
        self.pattern = pattern
        self.files = []
        self.index = 0

    def pop_dir(self):
        self.directory = self.dir_stack.pop()
        self.files = os.listdir(self.directory)
        self.index = 0

        if "scons" in self.directory:
            self.pop_dir()

    def __getitem__(self, index):
        while 1:
            try:
                file = self.files[self.index]
                self.index = self.index + 1
            except IndexError:
                self.pop_dir()
            else:
                fullname = os.path.join(self.directory, file)
                if os.path.isdir(fullname) and not os.path.islink(fullname):
                    self.dir_stack.append(fullname)
                if fnmatch.fnmatch(file, self.pattern):
                    return fullname

# -----------------------------------------------------------------------------

def sys_command(command):
    print(command)
    return_status = os.system(command)
    if return_status != 0:
        print("An error occured")
        sys.exit(0)

# -----------------------------------------------------------------------------

which_os = "linux"

print(">> Operating system : " + which_os)

env = Environment()

env.Decider("MD5-timestamp")

# env.Append(PATH=os.environ["PATH"])
env["ENV"]["PATH"] = os.environ.get("PATH")
env["ENV"]["PKG_CONFIG_PATH"] = os.environ.get("PKG_CONFIG_PATH")

if "CXX" in os.environ:
    print(">> Using compiler " + os.environ["CXX"])
    env.Replace(CXX=os.environ["CXX"])

# check build style
build_type = ARGUMENTS.get("config", "release")
if build_type != "release" and build_type != "debug":
    print("!! Unknown build config " + build_type)
    sys.exit(0)

print(">> Build type : " + build_type)

# check renderer
if which_os == "macosx":
    renderer = ARGUMENTS.get("renderer", "opengl")
else:
    renderer = ARGUMENTS.get("renderer", "wxwidgets")
if renderer != "opengl" and renderer != "wxwidgets":
    print("!! Unknown renderer " + renderer)
    sys.exit(0)

print(">> Renderer : " + renderer)
if renderer == "opengl":
    env.Append(CCFLAGS=["-DRENDERER_OPENGL"])
elif renderer == "wxwidgets":
    env.Append(CCFLAGS=["-DRENDERER_WXWIDGETS"])

# Check architecture
compiler_arch = ARGUMENTS.get("compiler_arch",
                              platform.architecture(env["CXX"]))[0]
if compiler_arch != "32bit" and compiler_arch != "64bit":
    print("Invalid architecture : " + compiler_arch + "; assuming 32bit")
    compiler_arch = "32bit"

print(">> Architecture : " + compiler_arch)

# add wxWidgets flags
# check if user defined his own WXCONFIG, else use defaults
WXCONFIG = ARGUMENTS.get("WXCONFIG", "wx-config")

wxver = subprocess.check_output(["wx-config", "--version"]).strip()
print(">> wxWidgets version : " + wxver)

wxlibs = None

if wxver[0] == "3" or (wxver[0] == "2" and wxver[2] == "9"):
    if renderer == "opengl":
        wxlibs = "core,base,adv,gl,net,webview"
    else:
        wxlibs = "core,base,adv,net,webview"
else:
    if renderer == "opengl":
        wxlibs = "core,base,adv,net,gl"
    else:
        wxlibs = "core,net,adv,base"

env.ParseConfig(["wx-config", "--cppflags", "--libs", wxlibs])

# check build type and init build flags
if build_type == "debug":
    env.Append(CCFLAGS=[
        "-g",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-D_MORE_DEBUG_CHECKS",
        "-D_CHECK_FOR_LEAKS",
        "-Wfatal-errors",
        "-DDEBUG=1"
    ])
elif build_type == "release":
    env.Append(CCFLAGS=["-O2", "-DNDEBUG=1"])
else:
    print("Unknown build type, cannot continue")
    sys.exit(0)

# init common header search paths
env.Append(CPPPATH=["./Src", ".", "./libjdkmidi/include", "./rtmidi"])

print(" ")

# add common sources
print("*** Adding source files")

sources = []
for f in RecursiveGlob(".", "*.cpp"):
    sources = sources + [f]

# add additional flags if any
user_flags = ARGUMENTS.get("CXXFLAGS", 0)
if user_flags != 0:
    env.Append(CCFLAGS=Split(user_flags))
user_flags = ARGUMENTS.get("LDFLAGS", 0)
if user_flags != 0:
    env.Append(LINKFLAGS=Split(user_flags))

# *************************************************************************
# **** PLATFORM SPECIFIC **************************************************
# *************************************************************************

use_jack = ARGUMENTS.get("jack", False)

print("*** Adding Alsa libraries and defines")

if renderer == "opengl":
    env.Append(CCFLAGS=["-DwxUSE_GLCANVAS=1"])

env.Append(CCFLAGS=["-D_ALSA"])

env.Append(CPPPATH=["/usr/include"])

if compiler_arch == "64bit":
    env.Append(CCFLAGS=["-D__X86_64__"])

if renderer == "opengl":
    env.Append(LIBS=["GL", "GLU"])

env.Append(LIBS=["asound"])
env.Append(LIBS=["dl", "m"])
env.ParseConfig("pkg-config --cflags glib-2.0")
env.ParseConfig("pkg-config --libs glib-2.0")

if use_jack:
    env.Append(CCFLAGS=["-DUSE_JACK"])
    env.Append(LIBS=["jack"])

# *************************************************************************
# **** COMPILE ************************************************************
# *************************************************************************

print("")
print("=====================")
print("     Setup done      ")
print("=====================")
print("")

# compile to .o
object_list = env.Object(source=sources)

# link program
executable = env.Program(target="Aria", source=object_list)

# install target
if "install" in COMMAND_LINE_TARGETS:

    # check if user defined his own prefix, else use defaults
    prefix = ARGUMENTS.get("prefix", 0)

    if prefix == 0:
        print(">> No prefix specified, defaulting to /usr/local/")
        prefix = "/usr/local/"
    else:
        print(">> Prefix : " + prefix)

    # set umask so created directories have the correct permissions
    try:
        os.umask("022")
    except OSError:  # ignore on systems that don't support umask
        pass

    bin_dir = os.path.join(prefix, "bin")
    data_dir = os.path.join(prefix, "share/Aria")
    locale_dir = os.path.join(prefix, "share/locale")

    if not os.path.exists(prefix):
        Execute(Mkdir(prefix))

    # install executable
    executable_target = bin_dir + "/Aria"
    env.Alias("install", executable_target)
    env.Command(executable_target, executable,
                [
                    Copy("$TARGET", "$SOURCE"),
                    Chmod("$TARGET", "0775"),
                ])

    # install data files
    data_files = []
    for f in RecursiveGlob("./Resources", "*"):
        if ".svn" in f or ".icns" in f or "*" in f:
            continue

        index = f.find("Resources/") + len("Resources/")
        filename_relative = f[index:]
        source = os.path.join("./Resources", filename_relative)
        target = os.path.join(data_dir, filename_relative)

        env.Alias("install", target)
        env.Command(target, source,
                    [
                        Copy("$TARGET", "$SOURCE"),
                        Chmod("$TARGET", "0664"),
                    ])

    # install .mo files
    mo_files = Glob("./international/*/aria_maestosa.mo", strings=True)
    for mo in mo_files:
        index_lo = mo.find("international/") + len("international/")
        index_hi = mo.find("/aria_maestosa.mo")
        lang_name = mo[index_lo:index_hi]
        fp = locale_dir + "/" + lang_name + "/LC_MESSAGES/aria_maestosa.mo"
        env.Alias("install", env.InstallAs(fp, mo))

# -----------------------------------------------------------------------------

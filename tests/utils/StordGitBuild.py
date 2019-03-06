#!/usr/bin/python3

import sys
import datetime
import shlex
import subprocess
import os
import re
import time
import signal
import multiprocessing

program_name = sys.argv[0]
branch = sys.argv[1:]

def Log(cmd, rc, out, err):
    print(cmd, rc, out, err)

def RunCommand(directory, cmd):
    out_file = "/tmp/cmd.out"
    err_file = "/tmp/cmd.err"

    cwd = os.getcwd()
    print("cwd = %s" % cwd)

    args = shlex.split(cmd)
    out_fh = open(out_file, "w")
    err_fh = open(err_file, "w")
    try:
        os.chdir(directory)
        print("cwd = %s" % os.getcwd())
        process = subprocess.Popen(args, cwd=directory, stdout=out_fh, stderr=err_fh)
        process.wait()
    except Exception as e:
        os.chdir(cwd)
        raise e
    os.chdir(cwd)
    print("cwd = %s" % os.getcwd())
    with open(out_file) as fh:
        o = fh.readlines()
    with open(err_file) as fh:
        e = fh.readlines()
    rc = process.returncode
    print("cwd = ", directory)
    Log(cmd, rc, o, e)
    if rc:
        raise Exception("Executing command ('%s') failed with error = %d" % (cmd, rc))
    return (rc, o, e)

'''
def RunCommand(cmd):
    args = shlex.split(cmd)
    process = subprocess.Popen(agrs, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    process.wait()
    o = process.stdout
    e = process.stderr
    rc = process.returncode
    Log(cmd, rc, o, e)
    return (rc, o, e)
'''

def IsExecutable(program):
    print("is program executable %s" % (program))
    return os.path.exists(program) and os.access(program, os.X_OK) and os.path.isfile(program)

class Git:
    def __init__(self, directory, repo, branch):
        self._branch = branch
        self._dir = directory
        self._repo = repo
        self._repo_url = "git@github.com:CacheboxInc/%s.git" % self._repo

    def ParentDir(self):
        return self._dir + "/"

    def RepoDir(self):
        return self.ParentDir() + self._repo + "/"

    def RepoUrl(self):
        return self._repo_url

    def UpdateRemote(self):
        cmd = "git remote update"
        return RunCommand(self.RepoDir(), cmd)

    def GetHeadCommitHash(self, branch):
        cmd = "git rev-parse %s" % branch
        (rc, o, e) = RunCommand(self.RepoDir(), cmd)
        assert(len(o) == 1)
        return o[0]

    def HasNewCommit(self):
        remote_branch = "origin/%s" % self._branch

        self.UpdateRemote()
        local = self.GetHeadCommitHash(self._branch)
        remote = self.GetHeadCommitHash(remote_branch)
        (rc, base, e) = RunCommand(self.RepoDir(), "git merge-base %s %s" % (self._branch, remote_branch))
        assert(len(base) == 1)

        if local == remote:
            # already up-to-date
            return False
        elif local == base[0]:
            # need a pull
            return True
        elif remote == base[0]:
            # we need a push from here
            return False
        else:
            # remote and local have diverged
            assert(False)
            return False

    def Pull(self):
        cmd = "git pull origin %s" % self._branch
        RunCommand(self.RepoDir(), cmd)
        self.PullSubmodules()

    def PullSubmodules(self):
        cmd = "git submodule init"
        RunCommand(self.RepoDir(), cmd)

        cmd = "git submodule update"
        RunCommand(self.RepoDir(), cmd)

    def Clone(self):
        if not self.IsCloned():
            cmd = "git clone %s" % self._repo_url
            RunCommand(self.ParentDir(), cmd)
        else:
            self.Pull()
        self.PullSubmodules()

    def GetCurrentBranch(self):
        try:
            cmd = "git branch"
            (rc, output, e) = RunCommand(self.RepoDir(), cmd)
            expr = re.compile("\*\s+(\w+)")
            for line in output:
                print(line)
                match = expr.match(line)
                if not match:
                    continue
                return match.group(1)
            assert(0)
        except Exception as e:
            print("Not a git repository")
            raise e

    def IsCloned(self):
        rc = os.path.isdir(self.RepoDir())
        if not rc:
            print("Directory not present %")
            return False
        try:
            b = self.GetCurrentBranch()
            assert(len(b) > 0)
            return True
        except:
            print("failed")
            return False

    def CheckoutBranch(self):
        cmd = "git checkout %s" % (self._branch)
        RunCommand(self.RepoDir(), cmd)

class CMakeBuildSystem:
    def __init__(self, build_type, cmake_flags, directory, commit_hash="", cpus=1):
        self._dir = directory
        self._commit = commit_hash
        self._build_type = build_type
        self._cpus = cpus
        self._cmake_flags = cmake_flags
        self.InitializeBuildDirectory()
        self.WriteCommitHash()

    def WriteCommitHash(self):
        if len(self._commit) <= 0:
            return
        with open("%s/commit" % self._dir, "w") as fh:
            fh.write(self._commit)
            fh.flush()

    def InitializeBuildDirectory(self):
        try:
            os.mkdir(self._dir)
        except FileExistsError as e:
            pass
        except Exception as e:
            print("Failed to create directory %s" % (self._dir))
            raise e

    def CMake(self):
        cmd = "cmake %s -DCMAKE_BUILD_TYPE=%s .." % (self._cmake_flags, self._build_type)
        RunCommand(self._dir, cmd)

    def Compile(self):
        cmd = "make -j %d" % (self._cpus)
        RunCommand(self._dir, cmd)

    def Install(self):
        cmd = "sudo make install"
        RunCommand(self._dir, cmd)

class MakeBuildSystem:
    def __init__(self, directory, cpus = 1):
        self._dir = directory
        self._cpus = cpus

    def Make(self):
        cmd = "make -j %d" % (self._cpus)
        print("Running command ", cmd)
        RunCommand(self._dir, cmd)

    def Install(self):
        cmd = "sudo make install"
        RunCommand(self._dir, cmd)

class HaLibBuild:
    def __init__(self, directory):
        self._dir = directory

    def Compile(self):
        self.BuildThridParty()
        build_dir = self._dir + "/build/"
        build = CMakeBuildSystem("Release", cmake_flags="", directory=build_dir)
        build.CMake()
        build.Compile()
        build.Install()

    def BuildThridParty(self):
        d = self._dir + "/" + "third-party/"
        make = MakeBuildSystem(d, 1)
        make.Make()

class StordBuild:
    def __init__(self, directory, branch, build_type):
        self._branch = branch
        self._repo = "hyc-storage-layer"
        self._build_type = build_type
        self._git = Git(directory, self._repo, branch)
        self._parent_dir = directory
        self._executable = None

    def RepoDir(self):
        return self._git.RepoDir()

    def Clone(self):
        self._git.Clone()
        self._git.CheckoutBranch()

    def HasNewCommit(self):
        return self._git.HasNewCommit()

    def BuildDirPath(self, build_dir_name):
        return self.RepoDir() + build_dir_name + "/"

    def Build(self, build_dir_name):
        path = self.BuildDirPath(build_dir_name)
        self._executable = path + "/src/stord/stord"
        commit = self._git.GetHeadCommitHash(self._branch)
        self.BuildHaLib()
        build = CMakeBuildSystem(self._build_type, cmake_flags="-DUSE_NEP=OFF", directory=path, commit_hash=commit, cpus=8)
        build.CMake()
        build.Compile()
        self.ExecutablePath()

    def ExecutablePath(self):
        assert(self._executable and IsExecutable(self._executable))
        return self._executable

    def BuildHaLib(self):
        path = self.RepoDir() + "/thirdparty/ha-lib/"
        build = HaLibBuild(path)
        build.Compile()

class HycCommonBuild:
    def __init__(self, directory, branch, build_type):
        self._parent_dir = directory
        self._branch = branch
        self._build_type = build_type
        self._repo = "HycStorCommon"
        self._git = Git(directory, self._repo, branch)

    def RepoDir(self):
        return self._git.RepoDir();

    def Clone(self):
        self._git.Clone()
        self._git.CheckoutBranch()

    def HasNewCommit(self):
        return self._git.HasNewCommit()

    def Build(self, build_dir_name):
        path = self.RepoDir() + "/" + build_dir_name + "/"
        commit = self._git.GetHeadCommitHash(self._branch)
        build = CMakeBuildSystem(self._build_type, cmake_flags="", directory=path, commit_hash=commit, cpus=7)
        build.CMake()
        build.Compile()
        build.Install()

class TgtBuild:
    def __init__(self, directory, branch, build_type):
        self._branch = branch
        self._repo = "tgt"
        self._build_type = build_type
        self._git = Git(directory, self._repo, branch)
        self._parent_dir = directory
        self._hyc_common_build = HycCommonBuild(directory, branch, build_type)
        self._executable = None

    def Clone(self):
        self._hyc_common_build.Clone()
        self._git.Clone()

    def RepoDir(self):
        return self._git.RepoDir()

    def HasNewCommit(self):
        return self._git.HasNewCommit() or self._hyc_common_build.HasNewCommit()

    def BuildDirPath(self, build_dir_name):
        return self.RepoDir()

    def Build(self, build_dir_name):
        self._executable = self.BuildDirPath(build_dir_name) + "/usr/tgtd"
        self._hyc_common_build.Build(build_dir_name)
        self.BuildHaLib()
        build = MakeBuildSystem(self.RepoDir(), 1)
        build.Make()
        self.ExecutablePath()

    def ExecutablePath(self):
        assert(self._executable and IsExecutable(self._executable))
        return self._executable

    def BuildHaLib(self):
        path = self.RepoDir() + "/thirdparty/ha-lib/"
        build = HaLibBuild(path)
        build.Compile()

class Process(object):
    def __init__(self):
        self._process = None

    def Run(self, program, args):
        def RunProgram(program, args):
            os.execv(program, args)
        assert(not self._process or not self._process.is_alive())
        self._process = multiprocessing.Process(target=RunProgram, args=(program, args))
        self._process.start()

    def Pid(self):
        if not self.IsRunning():
            return -1
        return self._process.pid

    def IsRunning(self):
        if self._process == None:
            return False
        return self._process.is_alive()

    def Crash(self):
        if not self._process.is_alive():
            return
        self._process.terminate()
        count = 0
        while count < 5 and self._process.is_alive():
            time.sleep(1)
            count += 1
        if count == 5 and self._process.is_alive():
            raise Exception("Not able to stop STORD.")

class Stord(Process):
    def __init__(self, directory, branch, build_type, args=()):
        self._build = StordBuild(directory, branch, build_type)
        self._args = args
        super(Stord, self).__init__()

    def Run(self):
        super(Stord, self).Run(self._build.ExecutablePath(), self._args)

    def Clone(self):
        self._build.Clone()

    def Build(self, build_dir_name):
        self._build.Build(build_dir_name)

    def HasNewCommit(self):
        return self._build.HasNewCommit()

class Tgtd(Process):
    def __init__(self, directory, branch, build_type, args=()):
        self._build = TgtBuild(directory, branch, build_type)
        self._args = args
        super(Tgtd, self).__init__()

    def Run(self):
        super(Tgtd, self).Run(self._build.ExecutablePath(), self._args)

    def Clone(self):
        self._build.Clone()

    def Build(self, build_dir_name):
        self._build.Build(build_dir_name)

    def HasNewCommit(self):
        return self._build.HasNewCommit()

class Etcd(Process):
    def __init__(self, path):
        self._program = path
        self._args = (" ", )
        super(Etcd, self).__init__()

    def Run(self):
        super(Etcd, self).Run(self._program, self._args)

def BuildDirName():
    return "build" + datetime.datetime.now().strftime("%Y-%m-%d-%H:%M")

tgtd_args = '-f -e "http://127.0.0.1:2379" -s "tgt_svc" -v "v1.0" -p 9001 -D "127.0.0.1" -P 9876'.split()
stord_args = '-etcd_ip="http://127.0.0.1:2379" -stord_version="v1.0" -svc_label="stord_svc" -ha_svc_port=9000 -v 1'.split()
if __name__ == "__main__":
    build_dir_name = BuildDirName()
    etcd = Etcd("/home/prasad/etcd/etcd")
    etcd.Run()
    print("STARTED ETCD")

    print("Building STORD")
    stord = Stord("/home/prasad/", "master", "Release", stord_args)
    stord.Clone()
    stord.Build(build_dir_name)

    print("Building TGTD")
    tgtd = Tgtd("/home/prasad/", "master", "Release", tgtd_args)
    tgtd.Clone()
    tgtd.Build(build_dir_name)

    iteration = 0
    while True:
        iteration += 1
        print("Iteration = %d" % (iteration))
        if not stord.IsRunning():
            stord.Run()

        if not tgtd.IsRunning():
            tgtd.Run()

        print("TGTD = %d, STORD = %d ETCD = %d" % (tgtd.Pid(), stord.Pid(), etcd.Pid()))
        time.sleep(60)

        build_dir_name = BuildDirName()
        stord_rebuilt = False
        if stord.HasNewCommit() == True:
            stord.Clone()
            stord.Build(build_dir_name)
            stord_rebuilt = True

        if tgtd.HasNewCommit() == True:
            tgtd.Clone()
            tgtd.Build(build_dir_name)

        if stord_rebuilt:
            stord.Crash()

        tgtd.Crash()

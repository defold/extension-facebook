#!/usr/bin/env python
import os, sys, shutil, subprocess

def run(cmd, cwd=None):
	print "CMD", cmd
	print "CWD", cwd
	p = subprocess.Popen(cmd.split(), cwd=cwd)
	p.wait()

def unpack_jar(path, outputdir):
	run('jar xf %s' % path, outputdir)

def pack_jar(path, outputjar):
	run('jar cf %s .' % outputjar, path)

def read_excludes(path):
	excludes = []
	with open(path, 'rb') as f:
		for line in f.readlines():
			line = line.strip()
			if not line.endswith('.class'):
				continue
			excludes.append(line)
	return excludes

def prune_files(dir, excludes):
	for root, dirs, files in os.walk(dir):
		for f in files:
			full = os.path.join(root, f)
			relative = os.path.relpath(full, dir)
			if relative in excludes:
				print relative, "\t\tEXCLUDED"
				os.unlink(full)


if __name__ == '__main__':
	injar = os.path.abspath(sys.argv[1])
	excludesfile = os.path.abspath(sys.argv[2])
	outputjar = os.path.abspath(sys.argv[3])
	tmpdir = os.path.abspath('_tmp')
	print "Created temp dir", tmpdir
	if not os.path.exists(tmpdir):
		os.makedirs(tmpdir)
	unpack_jar(injar, tmpdir)
	excludes = read_excludes(excludesfile)
	prune_files(tmpdir, excludes)
	pack_jar(tmpdir, outputjar)

	shutil.rmtree(tmpdir)
	print "Removed temp dir", tmpdir

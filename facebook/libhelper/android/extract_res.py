import os
import sys
import shutil

def copytree(src, dst):
    """ https://stackoverflow.com/a/13814557/468516 """
    if not os.path.exists(dst):
        os.makedirs(dst)
    for item in os.listdir(src):
        s = os.path.join(src, item)
        d = os.path.join(dst, item)
        if os.path.isdir(s):
            copytree(s, d)
        else:
            if not os.path.exists(d) or os.stat(s).st_mtime - os.stat(d).st_mtime > 1:
                shutil.copy2(s, d)

def extract_res(package_name, dir, output_dir):
    res_path = os.path.join(dir, "res")
    if os.path.exists(res_path):
        if os.listdir(res_path):
            print("Copying " + package_name)
            output_package_dir = os.path.join(output_dir, package_name)
            copytree(res_path, output_package_dir)


def iterate_and_extract(dir, output_dir):
    for package_name in os.listdir(dir):
        path = os.path.join(dir, package_name)
        if os.path.isdir(path):
            extract_res(package_name, path, output_dir)

# python extract_res.py build/result/packed/aar /output_directory/
def main():
    iterate_and_extract(sys.argv[1], sys.argv[2])

if __name__ == "__main__":
    main()

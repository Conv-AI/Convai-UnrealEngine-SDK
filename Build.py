import os
import sys
import subprocess
import zipfile
import io
import importlib.util
import argparse
import glob
import shutil

def is_git_repository(path):
    try:
        subprocess.run(["git", "-C", path, "status"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        return True
    except subprocess.CalledProcessError:
        return False

def clone_repo(repo_url, repo_directory):
    if not os.path.exists(repo_directory):
        os.makedirs(repo_directory)
        subprocess.run(["git", "clone", repo_url, repo_directory], check=True)

def install_gdown():
    """Installs gdown using pip."""
    subprocess.check_call([sys.executable, "-m", "pip", "install", "gdown"])

def download_file_from_gdrive(file_id, destination):
    """
    Downloads a file from Google Drive given its file ID using gdown.

    Args:
    file_id (str): The Google Drive file ID.
    destination (str): The destination file path.
    """
    if not importlib.util.find_spec('gdown'):
        install_gdown()
        importlib.reload(importlib)

    import gdown
    url = f'https://drive.google.com/uc?id={file_id}'
    gdown.download(url, destination, quiet=False)

def unzip_file(zip_path, destination):
    """
    Unzips a file to the specified destination.

    Args:
    zip_path (str): The path to the ZIP file.
    destination (str): The path to the destination folder.
    """
    if not os.path.exists(destination):
        os.makedirs(destination)

    with zipfile.ZipFile(zip_path, 'r') as zip_ref:
        zip_ref.extractall(destination)

def download_and_unzip(file_id, destination):
    download_file_from_gdrive(file_id, destination + '.zip')
    unzip_file(destination + '.zip', destination)

def build_plugin(ue_directory, plugin_directory, output_directory, extra_flags):
    plugin_file = next(glob.iglob(f'{plugin_directory}/**/*.uplugin', recursive=True), None)
    if not plugin_file:
        raise FileNotFoundError("No .uplugin file found in the plugin directory")

    print(f"Building Plugin: {plugin_file}")

    # Ensure the paths are correctly formatted
    plugin_file = os.path.abspath(plugin_file)
    ue_build_batch_file = os.path.join(ue_directory, "Engine", "Build", "BatchFiles", "RunUAT.bat")

    # Construct the build command with extra flags
    build_command = f'"{ue_build_batch_file}" BuildPlugin -Plugin="{plugin_file}" -Package="{output_directory}" -Rocket -Marketplace {extra_flags}'
    print(build_command)

    result = subprocess.run(build_command, shell=True)
    if result.returncode != 0:
        raise Exception("An error occurred during the build process")

def main(ue_directory, repo_directory = None, extra_flags = ""):
    if (repo_directory is None):
        repo_directory = os.getcwd()  # Assuming the script is in the repo directory

    # Update the repository
    subprocess.run(["git", "pull"], check=True)

    # Delete existing Content and ThirdParty directories
    content_dir = os.path.join(repo_directory)
    thirdparty_dir = os.path.join(repo_directory, "Source")
    output_dir = os.path.join(repo_directory, "Output")

    if os.path.exists(os.path.join(content_dir, "Content")):
        shutil.rmtree(os.path.join(content_dir, "Content"))
    if os.path.exists(os.path.join(thirdparty_dir, "ThirdParty")):
        shutil.rmtree(os.path.join(thirdparty_dir, "ThirdParty"))
    # if os.path.exists(output_dir):
    #     shutil.rmtree(output_dir)

    # Download and unzip new Content and ThirdParty directories
    download_and_unzip('1CE8J24bkLCdhUiY3kVYgJYB-3cANlaEq', content_dir)
    download_and_unzip('19slSAWhl9B_Uw92WAvL7FI_RTsKhd787', thirdparty_dir)

    # Build the plugin
    output_directory = os.path.join(repo_directory, "Output", os.path.basename(repo_directory))
    if not os.path.exists(output_directory):
        os.makedirs(output_directory)
    build_plugin(ue_directory, repo_directory, output_directory, extra_flags)

if __name__ == "__main__":
    current_directory = os.getcwd()
    if is_git_repository(current_directory):
        if len(sys.argv) > 1:
            ue_directory = sys.argv[1]
            extra_flags = ' '.join(sys.argv[2:])  # Treat all other args as extra flags
        else:
            ue_directory = input("Enter the Unreal Engine directory path: ")
            extra_flags = ''
        repo_directory = current_directory
    else:
        if len(sys.argv) > 2:
            ue_directory = sys.argv[1]
            repo_url = sys.argv[2]
            extra_flags = ' '.join(sys.argv[3:])  # Gather extra flags
            repo_name = os.path.basename(repo_url)
            repo_directory = os.path.join(current_directory, repo_name)
            clone_repo(repo_url, repo_directory)
            os.chdir(repo_directory)
        elif len(sys.argv) > 1:
            ue_directory = sys.argv[1]
            extra_flags = ' '.join(sys.argv[2:])  # Gather extra flags
            repo_directory = current_directory
        else:
            ue_directory = input("Enter the Unreal Engine directory path: ")
            extra_flags = ''
            repo_directory = current_directory

    main(ue_directory, repo_directory, extra_flags)
import os

Import("env")

try:
    import dotenv
except ImportError:
    # check if we are under archlinux
    if os.path.exists('/usr/bin/pacman'):
        print("Please install python-dotenv via 'sudo pacman -S python-dotenv'")
        exit(1)
    print("Installing python-dotenv via pip...")
    env.Execute("$PYTHONEXE -m pip install python-dotenv")

from dotenv import load_dotenv

load_dotenv('.env')

for key, value in os.environ.items():
    if key.startswith('PIO_'):
        env.Append(CPPDEFINES=[(key, value)])
        print(f"Added define: {key}={value}")

# print out all defines
print("Current CPPDEFINES:")
for define in env.get("CPPDEFINES", []):
    print(f"  {define}")
Import("env")

try:
    import dotenv
except ImportError:
    env.Execute("$PYTHONEXE -m pip install dotenv")

from dotenv import load_dotenv
import os
load_dotenv()

print("WiFI SSID: ",  os.getenv("WIFI_SSID"))
print("WiFI Password: ",  os.getenv("WIFI_PASSWORD"))

env.Append(CPPDEFINES=[
    ("WIFI_SSID", env.StringifyMacro(os.getenv("WIFI_SSID"))),
    ("WIFI_PASSWORD", env.StringifyMacro(os.getenv("WIFI_PASSWORD"))),
])
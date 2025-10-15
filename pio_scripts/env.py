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
    ("OTA_SERVER_BASE_URL", env.StringifyMacro(os.getenv("OTA_SERVER_BASE_URL"))),
    ("OTA_PASSWORD", env.StringifyMacro(os.getenv("OTA_PASSWORD"))),
])
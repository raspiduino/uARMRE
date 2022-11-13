"C:\Program Files (x86)\MRE SDK V3.0.00\tools\DllPackage.exe" "D:\Data\dev\uARMRE\uARMRE.vcproj"
if %errorlevel% == 0 (
 echo postbuild OK.
  copy uARMRE.vpp ..\..\..\MoDIS_VC9\WIN32FS\DRIVE_E\uARMRE.vpp /y
exit 0
)else (
echo postbuild error
  echo error code: %errorlevel%
  exit 1
)


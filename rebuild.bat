@call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
@set T=Rebuild
for %%P in (Win32 x64) do (
	for %%C in (Debug Release) do (
		msbuild mp4.sln /t:%T% /p:Platform="%%~P";Configuration="%%~C"
	)
)
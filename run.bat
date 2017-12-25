@echo off

echo change vedio to bmp
echo -----------------

set /p filename=vedio:
set /p output=outputfile:
set /p fps=FPS:

ffmpeg -i "%filename%" -r %fps% -vcodec bmp "%output%"

pause
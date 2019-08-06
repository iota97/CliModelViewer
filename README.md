# CliModelViewer

CLI mesh rasterizer - Use with .obj model format

Build with: 

"cc -O2 mesh_viewer.c -o mesh_viewer" for normal mode
"cc -O2 mesh_viewer.c -o mesh_viewer -lncurses -DNCURSES" for NCURSES mode


Syntax: mesh_viewer [path/to/mesh.obj]


Normal mode command syntax:

	t[axis] [ammount] - translate
	r[axis] [ammount] - rotate
	s[axis] [ammount] - scale
	h - help
	m - reset
	q - quit
	v [widht]x[height] - set viewport size

	axis: x, y, z, a - all (scale only)
						
	ammount: float value
								
	Examples: 'tx -0.2' translate on x axis by -0.2 
		  'ry 90' rotate on y axis by 90 degree	
		  'sa 0.5' scale all the axis by half	
		  'v 80x24' set vieport to 80x24 (default)
								
	Press [Enter] to repeat the last command


Ncurses mode command list:
									
	Move:		W - up		A - left	Z - forward	
			S - down	D - right	X - backward
									
	Rotate: 	I, K - on X axis	
			J, L - on Y axis	
			U, O - on Z axis	
									
	Scale:		+, -
								
	Misc: 		R - reset	Q - quit	H - help

# CliModelViewer

CLI mesh rasterizer - Use with .obj model format

	Syntax: mesh_viewer [path/to/mesh.obj]
		
	In program syntax: [action][axis] [ammount]
								
			action: t - translate				
				r - rotate			
				s - scale				
				q - quit				
				m - reset
				h - help				
									
			axis: x, y, z, a - all (scale only) 		
									
			ammount: float value				
									
			Examples: 'tx -0.2' translate on x axis by -0.2 
				  'ry 1' rotate on y axis of 1 rad
				  'sa 0.5' scale all the axis by half
				  'm' reset mesh to start status

			Press [Enter] to repeat the last command
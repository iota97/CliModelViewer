// CLI rasterization

// use tris only .obj model with far away vertex (float precision)

#include <stdio.h>
#include <math.h>

// Help message
char *help_message =
	"CLI mesh rasterizer - Use with .obj model made only of tris	\n\
									\n\
	Syntax: mesh_viewer [path/to/mesh.obj]				\n\
									\n\
	In program syntax: [action][axis] [ammount]			\n\
									\n\
			action: t - translate				\n\
				r - rotate				\n\
				s - scale				\n\
				q - quit				\n\
				m - reset				\n\
				h - help				\n\
									\n\
			axis: x, y, z, a - all (scale only) 		\n\
									\n\
			ammount: float value				\n\
									\n\
			Examples: 'tx -0.2' translate on x axis by -0.2 \n\
				  'ry 1' rotate on y axis of 1 rad	\n\
				  'sa 0.5' scale all the axis by half	\n\
				  'm' reset mesh to start status	\n";

// Vertex stuct
typedef struct vertex 
{
	float x;
	float y;
	float z;
} vertex_t;


// A buffer with all tris
static vertex_t tris_buffer[8192][3];
static vertex_t mesh_tris[8192][3];
static int tris_count = 0;

// Screen and depth buffer
static char screen[80][40];
static float depth[80][40];

// Material array of char
static char material_array[] = {'a', 'b', 'c', 'd', 
				'e', 'f', 'g', 'h',
				'i', 'j', 'k', 'l',
				'm', 'n', 'o', 'p'};

// Restore position
void restore()
{
	for (int i = 0; i < tris_count; i++) 
	{
		for (int j = 0; j < 3; j++) 
		{
			tris_buffer[i][j].x = mesh_tris[i][j].x; 
			tris_buffer[i][j].y = mesh_tris[i][j].y;
			tris_buffer[i][j].z = mesh_tris[i][j].z;
		}
	}

	return;
}

// Read the mesh file
int parse_obj(char* path) 
{
	char line_buffer[1024];
	FILE* mesh_file = fopen(path, "r");

	// Check the read to be succefull
	if (mesh_file == NULL)
	{
		printf("Error reading file %s\n", path);
		return 1;
	}	

	int vertex_index = 0;
	vertex_t vertex_buffer[8192];

	while(fgets(line_buffer, 1024, (FILE*) mesh_file)) 
	{
		// Read vertex data
		if (line_buffer[0] == 'v' && line_buffer[1] == ' ') 
		{
			sscanf(line_buffer, "%*s %f %f %f", 
				&vertex_buffer[vertex_index].x, 
				&vertex_buffer[vertex_index].y, 
				&vertex_buffer[vertex_index].z);

			vertex_index++;
		}

		// Read face data
		if (line_buffer[0] == 'f' && line_buffer[1] == ' ') 
		{
			int a, b, c;
			sscanf(line_buffer, "%*s %u/%*s %u/%*s %u",
				 &a, &b, &c);

			// Bind the right vertex
			mesh_tris[tris_count][0] = vertex_buffer[a-1];
			mesh_tris[tris_count][1] = vertex_buffer[b-1];
			mesh_tris[tris_count][2] = vertex_buffer[c-1];
			
			// Increase the face count
			tris_count++;
		}

	}

	// Close the file
	fclose(mesh_file);
	return 0;
}

// Translate the mesh
void translate(float x, float y, float z) 
{
	for (int i = 0; i < tris_count; i++) 
	{
		for (int j = 0; j < 3; j++) 
		{
			tris_buffer[i][j].x += x; 
			tris_buffer[i][j].y += y;
			tris_buffer[i][j].z += z;
		}
	}

	return;
}

// Scale the mesh
void scale(float x, float y, float z)
{
	for (int i = 0; i < tris_count; i++) 
	{
		for (int j = 0; j < 3; j++) 
		{
			tris_buffer[i][j].x *= x; 
			tris_buffer[i][j].y *= y;
			tris_buffer[i][j].z *= z;
		}

	}

	return;
}

// Rotate the mesh
void rotate(float x, float y, float z)
{
	for (int i = 0; i < tris_count; i++) 
	{
		for (int j = 0; j < 3; j++) 
		{
			// Y
			tris_buffer[i][j].x = cos(y) * tris_buffer[i][j].x -
						sin(y) * tris_buffer[i][j].z;
			tris_buffer[i][j].z = sin(y) * tris_buffer[i][j].x +
						cos(y) * tris_buffer[i][j].z; 

			// X
			tris_buffer[i][j].y = cos(x) * tris_buffer[i][j].y -
						sin(x) * tris_buffer[i][j].z;
			tris_buffer[i][j].z = sin(x) * tris_buffer[i][j].y +
						cos(x) * tris_buffer[i][j].z; 

			// Z
			tris_buffer[i][j].x = cos(z) * tris_buffer[i][j].x -
						sin(z) * tris_buffer[i][j].y;
			tris_buffer[i][j].y = sin(z) * tris_buffer[i][j].x +
						cos(z) * tris_buffer[i][j].y; 
		}
	}

	return;
}

// Clear the screen and depth buffer
void clear()
{
	for (int i = 0; i < 80; i++)
	{
		for (int j = 0; j < 40; j++)
		{
			screen[i][j] = ' ';
			depth[i][j] = 8192;
		}
	}

	return;
}

// Render to screen buffer
void render()
{
	// Clear before start
	clear();
	int material_index = 0;

	for (int i = 0; i < tris_count; i++) 
	{
		
		// Raster the vertex to screen
		double x_array[] = {
			(tris_buffer[i][0].x / tris_buffer[i][0].z * 80) + 80/2,
			(tris_buffer[i][1].x / tris_buffer[i][1].z * 80) + 80/2,
			(tris_buffer[i][2].x / tris_buffer[i][2].z * 80) + 80/2
		};
		double y_array[] = {
			(tris_buffer[i][0].y / tris_buffer[i][0].z * 40) + 40/2,
			(tris_buffer[i][1].y / tris_buffer[i][1].z * 40) + 40/2,
			(tris_buffer[i][2].y / tris_buffer[i][2].z * 40) + 40/2
		};

		// Get the bounding coordinate of the tris
		int min_x = 80;
		int min_y = 40;
		int max_x = 0;
		int max_y = 0;

		for (int j = 0; j < 3; j++)
		{
			if (x_array[j] < min_x)
				min_x = x_array[j];
			if (x_array[j] > max_x)
				max_x = x_array[j];
			if (y_array[j] < min_y)
				min_y = y_array[j];
			if (y_array[j] > max_y)
				max_y = y_array[j];
		}
		
		// Check boundaries
		max_x = (max_x > 80) ? 80 : max_x;
		max_y = (max_y > 40) ? 40 : max_y;
		min_x = (min_x < 0) ? 0 : min_x;
		min_y = (min_y < 0) ? 0 : min_y;
		
		// Test only the pixel in this area
		for (int y = min_y; y < max_y; y++) 
		{
			for (int x = min_x; x < max_x; x++) 
			{
				int out = 0;
				
				for (int j = 0; j < 3; j++)
				{
					// Get the next vertex index
					int j_n = (j == 2) ? 0 : j + 1;	
					int j_nn = (j_n == 2) ? 0 : j_n + 1;

					// Handle vertical line
					if (x_array[j] == x_array[j_n])
					{	
						out += (x_array[j] < x_array[j_nn]) ? (x < x_array[j]) : (x > x_array[j]);
					}
					else 
					{ 
						// Handle horizzontal one
						if (y_array[j] == y_array[j_n])
						{
							out += (y_array[j] < y_array[j_nn]) ? (y < y_array[j]) : (y > y_array[j]);
						}						
						else
						{ 
							// Check clock wise or not
							if ((x_array[j_nn] - x_array[j])*(y_array[j_n] - y_array[j]) < 
								(y_array[j_nn] - y_array[j])*(x_array[j_n] - x_array[j]))
								
								// Then
								out += (x - x_array[j])*(y_array[j_n] - y_array[j]) > 
									(y - y_array[j])*(x_array[j_n] - x_array[j]);
							else 
								// Else
								out += (x - x_array[j])*(y_array[j_n] - y_array[j]) < 
									(y - y_array[j])*(x_array[j_n] - x_array[j]);
						}					
					}
				}

				// If is inside every line, render it
				if (!out) 
				{
					// Interpolate the depth
					float d=0;
					for (int j = 0; j < 3; j++)
					{
						d += tris_buffer[i][j].z/sqrt((x-x_array[j])*(x-x_array[j]) + (y-y_array[j])*(y-y_array[j]));
					}
					
					// Test it
					if (depth[x][y] > d && d < 1)
					{
						// Update both buffer
						screen[x][y] = material_array[material_index];
						depth[x][y] = d;
					}
				}

			}
			
		}

		// Render next face with another material
		material_index++;

		// Return to 0 if last element is reached
		if (material_index == sizeof(material_array)/sizeof(char))
			material_index = 0;

	}

	return;
}

// Clear the console
void clear_screen()
{	
	// Print 256 new line
	for (int i = 0; i < 256; i++)
		printf("\n");

	return;
}

// Draw in the console
void draw()
{
	// Render to buffer
	render();
	
	// Clear the console
	clear_screen();

	// Print it, cropping to 80x24
	for (int j = 12; j < 36; j++) 
	{
		for (int i = 0; i < 80; i++) 
		{
			printf("%c", screen[i][j]);
		}
		printf("\n");
	}

	return;
}

// Help message

void help()
{
	// Clear the console
	clear_screen();

	// Print usage instruction
	printf("%s\n", help_message);
	
	// Wait input to proceed
	getchar();
	return;
}

// Input loop
void loop ()
{
	// Translation (-5 in Z, for initial view)
	float t_x = 0;
	float t_y = 0;
	float t_z = -5;

	// Rotation
	float r_x = 0;
	float r_y = 0;
	float r_z = 0;

	// Scale
	float s_x = 1;
	float s_y = 1;
	float s_z = 1;

	// Input variables
	char input[128];
	char command[32];	
	float ammount = 0;
	int quit = 0;

	// Keep looping until user hit 'q'
	for(;;)
	{	
		// Restore the mesh
		restore();

		// Set scale, rotation and positon
		rotate(r_x, r_y, r_z);
		scale(s_x, s_y, s_z);
		translate(t_x, t_y, t_z);

		// Render it
		draw();

		// Get input
		fgets(input, 256, stdin);
		sscanf(input, "%s %f", command, &ammount);

		// Quit
		if (command[0] == 'q')
			break;

		// Translation
		else if (command[0] == 't')
		{
			
			if (command[1] == 'x')
				t_x += ammount;
			if (command[1] == 'y')
				t_y += ammount;
			if (command[1] == 'z')
				t_z += ammount;
		}

		// Rotation
		else if (command[0] == 'r')
		{
			
			if (command[1] == 'x')
				r_x += ammount;
			if (command[1] == 'y')
				r_y += ammount;
			if (command[1] == 'z')
				r_z += ammount;
		}

		// Scale
		else if (command[0] == 's')
		{
			
			if (command[1] == 'x')
				s_x *= ammount;
			if (command[1] == 'y')
				s_y *= ammount;
			if (command[1] == 'z')
				s_z *= ammount;
			if (command[1] == 'a')
			{
				s_x *= ammount;
				s_y *= ammount;
				s_z *= ammount;
			}
		}

		// Reset
		else if (command[0] == 'm')
		{
			t_x = 0;
			t_y = 0;
			t_z = -5;
			r_x = 0;
			r_y = 0;
			r_z = 0;
			s_x = 1;
			s_y = 1;
			s_z = 1;
		}

		// Print help
		else if (command[0] == 'h')
			help();	

		// Reset command
		command[0] = 0;	
	}

	return;
}

// Main
int main(int argc, char *argv[]) 
{

	// Check argument number
	if (argc < 2) 
	{		
		printf("Please provide the model path.\n");
		return 1;	
	}

	// Parse the model
	if (parse_obj(argv[1]))
		return 2;
	
	// Print help message at start
	help();	

	// Start the loop
	loop();

	// Exit
	return 0;
}
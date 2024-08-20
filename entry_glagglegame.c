typedef enum EntityArchetype{
	arch_nil = 0,
	arch_tree = 1,
	arch_rock = 2,
	arch_player = 3,

}EntityArchetype;

typedef struct Entity {
	bool is_valid;
	EntityArchetype arch;
	Vector2 pos;

} Entity;

#define MAX_ENTITY_COUNT 1024
typedef struct World {
	Entity entities[MAX_ENTITY_COUNT];

} World;
World* world = 0;

Entity* entity_create(){
	Entity* entity_found = 0;
	for(int i = 0; i < MAX_ENTITY_COUNT; i++){
		Entity* existing_entity = &world->entities[i];
		if(!existing_entity->is_valid){
			entity_found = existing_entity;
			break;
		}
	}
	assert(entity_found, "no more free entities");
	return entity_found;
}

void entity_destroy(Entity* entity){
	memset(entity, 0, sizeof(Entity));
}

int entry(int argc, char **argv) {
	
	window.title = STR("Glagglegame");
	window.scaled_width = 1280; // We need to set the scaled size if we want to handle system scaling (DPI)
	window.scaled_height = 720; 
	window.x = 200;
	window.y = 90;
	window.clear_color = hex_to_rgba(0xA9A9A9ff);

	world = alloc(get_heap_allocator(), sizeof(World));

	Entity* player_en = entity_create();
	/**************************************************
	 * figure out how to draw 10 trees next to each other 
	for (int i = 0; i < 10; i++){
		
	}
	**************************************************/
	Entity* tree_en = entity_create();

	float64 last_time = os_get_current_time_in_seconds();

	Gfx_Image* player = load_image_from_disk(STR("player.png"), get_heap_allocator());
	assert(player, "ya dun goofed.");
	Gfx_Image* tree0 = load_image_from_disk(STR("tree0.png"), get_heap_allocator());
	assert(tree0, "ya dun goofed.");


	while (!window.should_close) {
		// fps log, delta_t calculation
		float64 now = os_get_current_time_in_seconds();
		if ((int)now != (int)last_time) log("%.2f FPS\n%.2fms", 1.0/(now-last_time), (now-last_time)*1000);
		float64 delta_t = now - last_time;
		last_time = now;
		// adjusting zoom, scaling the window
		draw_frame.projection = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);
		float64 zoom = 5.3;
		draw_frame.view = m4_make_scale(v3(1.0/zoom, 1.0/zoom, 1.0));
		
		if(is_key_just_pressed(KEY_ESCAPE)){
			window.should_close = true;
		}
		// movement
		Vector2 input_axis = v2(0, 0);
		if (is_key_down('A')) {
			input_axis.x -= 1.0;
		}
		if (is_key_down('D')) {
			input_axis.x += 1.0;
		}
		if (is_key_down('S')) {
			input_axis.y -= 1.0;
		}
		if (is_key_down('W')) {
			input_axis.y += 1.0;
		}
		input_axis = v2_normalize(input_axis);
		// movement
		float64 movespeed = 100.0 * delta_t;
		player_en->pos = v2_add(player_en->pos, v2_mulf(input_axis, movespeed));
		// drawing the player
		reset_temporary_storage();

		{
			Vector2 size = v2(8.0, 8.0);
			Matrix4 xform = m4_scalar(1.0);
			xform         = m4_translate(xform, v3(player_en->pos.x, player_en->pos.y, 0));
			xform         = m4_translate(xform, v3(size.x *-0.5, 0, 0));
			draw_image_xform(player, xform, size, COLOR_RED);
		}

		{
			Vector2 size = v2(8.0, 12.0);
			Matrix4 xform = m4_scalar(1.0);
			xform         = m4_translate(xform, v3(0, 0, 0));
			draw_image_xform(tree0, xform, size, COLOR_GREEN);
		}
		
		os_update(); 
		gfx_update();
	}

	return 0;
}
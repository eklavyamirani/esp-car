// Ultrasonic Sensor (HC-SR04) Mount for Freenove 4WD Car
// Angled 45° downward for table edge detection

/* --- Parameters --- */

// Chassis mounting
hole_spacing    = 43.63;   // center-to-center M3 holes
hole_diameter   = 3.2;     // M3 clearance
base_depth      = 40;      // holes to front + overhang
base_width      = hole_spacing + 12; // padding around holes
base_thick      = 4;

// Riser
riser_height    = 90;
riser_width     = base_width;
riser_thick     = 4;

// HC-SR04 sensor
sensor_board_w  = 45;
sensor_board_h  = 20;
sensor_board_t  = 1.5;
sensor_can_d    = 16.2;    // transducer diameter + tolerance
sensor_can_spacing = 26;   // center-to-center of transducers
sensor_lip      = 1.5;     // lip to hold board in place

// Cradle
cradle_angle    = 45;
cradle_thick    = 3;
cradle_depth    = sensor_board_h + 4;

// Fillets/reinforcement
gusset_size     = 15;

/* --- Modules --- */

module base_plate() {
    difference() {
        // plate
        translate([-base_width/2, 0, 0])
            cube([base_width, base_depth, base_thick]);
        
        // M3 mounting holes
        for (x = [-hole_spacing/2, hole_spacing/2]) {
            translate([x, 10, -1])
                cylinder(h = base_thick + 2, d = hole_diameter, $fn = 32);
        }
    }
}

// Replace the solid riser with two posts + crossbar
post_width = 6;

module riser() {
    for (x = [-riser_width/2, riser_width/2 - post_width]) {
        translate([x, base_depth - riser_thick, base_thick])
            cube([post_width, riser_thick, riser_height]);
    }
    // crossbar at top
    translate([-riser_width/2, base_depth - riser_thick, base_thick + riser_height - post_width])
        cube([riser_width, riser_thick, post_width]);
}

module gusset() {
    for (x = [-riser_width/2, riser_width/2 - post_width]) {
        translate([x, base_depth - riser_thick - gusset_size, base_thick])
            rotate([0, 90, 0])
                linear_extrude(height = post_width)
                    polygon([
                        [0, 0],
                        [0, gusset_size],
                        [-gusset_size, gusset_size]
                    ]);
    }
}

module sensor_cradle() {
    translate([0, base_depth, base_thick + riser_height])
    rotate([cradle_angle, 0, 0])
    translate([-sensor_board_w/2 - 2, 0, 0]) {
        cradle_w = sensor_board_w + 4;
        
        // back wall
        cube([cradle_w, cradle_thick, sensor_board_h + 4]);
        
        // bottom lip
        cube([cradle_w, cradle_depth, cradle_thick]);
        
        // side walls
        for (x = [0, cradle_w - cradle_thick]) {
            translate([x, 0, 0])
                cube([cradle_thick, cradle_depth, sensor_board_h + 4]);
        }
        
        // front lip (shorter, to slide sensor in)
        translate([0, cradle_depth - cradle_thick, 0])
            cube([cradle_w, cradle_thick, sensor_lip + cradle_thick]);
        
        // cutouts for transducer cans (so they poke through bottom)
        translate([0, 0, -1]) {
            for (i = [-1, 1]) {
                translate([cradle_w/2 + i * sensor_can_spacing/2, cradle_depth/2, 0])
                    cylinder(h = cradle_thick + 2, d = sensor_can_d, $fn = 48);
            }
        }
    }
}

module cradle_support() {
    s = 20;
    for (x = [-riser_width/2, riser_width/2 - post_width]) {
        hull() {
            // top of post
            translate([x, base_depth - riser_thick, base_thick + riser_height])
                cube([post_width, 1, 1]);
            // same height, behind
            translate([x, base_depth - riser_thick - s, base_thick + riser_height])
                cube([post_width, 1, 1]);
            // down the post, flush with riser
            translate([x, base_depth - riser_thick, base_thick + riser_height - s])
                cube([post_width, 1, 1]);
        }
    }
}

/* --- Assembly --- */

module mount() {
    color("DodgerBlue") base_plate();
    color("SteelBlue") riser();
    color("SteelBlue") gusset();
    color("CornflowerBlue") sensor_cradle();
    cradle_support();
}

mount();


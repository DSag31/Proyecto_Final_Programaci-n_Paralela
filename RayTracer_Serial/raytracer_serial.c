
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <errno.h>

#define WIDTH 1280
#define HEIGHT 720
#define SAMPLES 16
#define MAX_DEPTH 3

#define NUM_SPHERES 6
#define NUM_MATERIALS 7

#define EPSILON 0.0001
#define PI 3.14159265358979323846

//  ESTRUCTURAS

typedef struct {
    double x, y, z;
} Vec3;

typedef struct {
    Vec3 origin;
    Vec3 direction;
} Ray;

typedef struct {
    Vec3 color;
    double reflectivity;
} Material;

typedef struct {
    Vec3 center;
    double radius;
    int material_id;
} Sphere;

typedef struct {
    Vec3 point;
    Vec3 normal;
    int material_id;
} Plane;

typedef struct {
    Vec3 position;
    Vec3 intensity;
} PointLight;

typedef struct {
    Vec3 position;
    Vec3 forward;
    Vec3 right;
    Vec3 up;
    double viewport_width;
    double viewport_height;
} Camera;

typedef struct {
    double t;
    Vec3 point;
    Vec3 normal;
    int material_id;
    int hit;
} HitRecord;

// VARIABLES GLOBALES DE LA ESCENA

Sphere spheres[NUM_SPHERES];
int sphere_count = NUM_SPHERES;

Material materials[NUM_MATERIALS];

Plane ground;
PointLight light;

// OPERACIONES DE VECTORES

Vec3 vec3(double x, double y, double z) {
    Vec3 v = {x, y, z};
    return v;
}

Vec3 add(Vec3 a, Vec3 b) {
    return vec3(a.x+b.x, a.y+b.y, a.z+b.z);
}

Vec3 sub(Vec3 a, Vec3 b) {
    return vec3(a.x-b.x, a.y-b.y, a.z-b.z);
}

Vec3 mul(Vec3 a, double k) {
    return vec3(a.x*k, a.y*k, a.z*k);
}

Vec3 hadamard(Vec3 a, Vec3 b) {
    return vec3(a.x*b.x, a.y*b.y, a.z*b.z);
}

double dot(Vec3 a, Vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

Vec3 cross(Vec3 a, Vec3 b) {
    return vec3(
        a.y*b.z-a.z*b.y,
        a.z*b.x-a.x*b.z,
        a.x*b.y-a.y*b.x
    );
}

double length_vec(Vec3 v) {
    return sqrt(dot(v,v));
}

Vec3 normalize(Vec3 v) {
    double len = length_vec(v);

    if (len < 1e-12)
        return vec3(0,0,0);

    return mul(v, 1.0/len);
}

Vec3 reflect(Vec3 d, Vec3 n) {
    return sub(d, mul(n, 2.0*dot(d,n)));
}

Vec3 clamp_color(Vec3 c) {
    if (c.x < 0) c.x = 0;
    if (c.y < 0) c.y = 0;
    if (c.z < 0) c.z = 0;

    if (c.x > 1) c.x = 1;
    if (c.y > 1) c.y = 1;
    if (c.z > 1) c.z = 1;

    return c;
}

// GENERADOR ALEATORIO DETERMINISTA

uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;

    return x;
}

double random01(uint32_t *state) {
    *state = hash32(*state + 0x9e3779b9);

    return (double)(*state) / 4294967296.0;
}

double random_range(uint32_t *state, double min, double max) {
    return min + random01(state)*(max-min);
}

int random_int(uint32_t *state, int min, int max) {
    return min + (int)(
        random01(state)*(max-min+1)
    );
}

// RAYOS

Ray make_ray(Vec3 origin, Vec3 direction) {
    Ray r;

    r.origin = origin;
    r.direction = normalize(direction);

    return r;
}

Vec3 ray_at(Ray r, double t) {
    return add(r.origin, mul(r.direction,t));
}

// INTERSECCION RAYO-ESFERA

int hit_sphere(
    Ray ray,
    Sphere sphere,
    double t_min,
    double t_max,
    HitRecord *record
) {
    Vec3 oc = sub(ray.origin, sphere.center);

    double a = dot(ray.direction, ray.direction);
    double b = 2.0*dot(oc,ray.direction);
    double c = dot(oc,oc)-sphere.radius*sphere.radius;

    double discriminant = b*b-4.0*a*c;

    if (discriminant < 0)
        return 0;

    double sqrtd = sqrt(discriminant);

    double root = (-b-sqrtd)/(2.0*a);

    if (root < t_min || root > t_max) {
        root = (-b+sqrtd)/(2.0*a);

        if (root < t_min || root > t_max)
            return 0;
    }

    record->t = root;
    record->point = ray_at(ray,root);

    record->normal = normalize(
        sub(record->point,sphere.center)
    );

    record->material_id = sphere.material_id;
    record->hit = 1;

    return 1;
}

// INTERSECCION RAYO-PLANO

int hit_plane(
    Ray ray,
    Plane plane,
    double t_min,
    double t_max,
    HitRecord *record
) {
    double denominator = dot(
        ray.direction,
        plane.normal
    );

    if (fabs(denominator) < 1e-10)
        return 0;

    double t = dot(
        sub(plane.point,ray.origin),
        plane.normal
    )/denominator;

    if (t < t_min || t > t_max)
        return 0;

    record->t = t;
    record->point = ray_at(ray,t);
    record->normal = normalize(plane.normal);
    record->material_id = plane.material_id;
    record->hit = 1;

    return 1;
}

// GENERACION DE ESCENAS ALEATORIAS

void initialize_scene(uint32_t seed) {

    uint32_t state = hash32(seed);

    sphere_count = NUM_SPHERES;


    // MATERIALES ALEATORIOS

    for (int i = 0; i < 6; i++) {

        materials[i].color = vec3(
            random_range(&state,0.15,0.95),
            random_range(&state,0.15,0.95),
            random_range(&state,0.15,0.95)
        );

        materials[i].reflectivity =
            random_range(&state,0.0,0.75);
    }

    // Material del suelo
    materials[6].color = vec3(0.65,0.65,0.65);
    materials[6].reflectivity = 0.0;

    // SUELO

    ground.point = vec3(0,-1,0);
    ground.normal = vec3(0,1,0);
    ground.material_id = 6;

    // ILUMINACION ALEATORIA

    light.position = vec3(
        random_range(&state,-6.0,6.0),
        random_range(&state,4.0,9.0),
        random_range(&state,1.0,7.0)
    );

    light.intensity = vec3(
        random_range(&state,0.9,1.5),
        random_range(&state,0.9,1.5),
        random_range(&state,0.9,1.5)
    );

    // GENERACION DE LAS SEIS ESFERAS

    for (int i = 0; i < NUM_SPHERES; i++) {

        double radius = random_range(
            &state,0.40,0.95
        );

        double x = 0.0;
        double z = 0.0;

        int valid_position = 0;

        // Buscar una posición sin superposición
        for (int attempt = 0; attempt < 1000; attempt++) {

            x = random_range(&state,-4.5,4.5);
            z = random_range(&state,-9.5,-1.5);

            valid_position = 1;

            for (int j = 0; j < i; j++) {

                double dx = x-spheres[j].center.x;
                double dz = z-spheres[j].center.z;

                double distance = sqrt(
                    dx*dx+dz*dz
                );

                double min_distance =
                    radius+spheres[j].radius+0.15;

                if (distance < min_distance) {
                    valid_position = 0;
                    break;
                }
            }

            if (valid_position)
                break;
        }

        // Posición alternativa si se agotan los intentos
        if (!valid_position) {

            x = -4.0+(i%3)*4.0;
            z = -3.0-(i/3)*5.0;
        }

        // Apoyar la esfera sobre el suelo
        double y = -1.0+radius;

        // Asignar material aleatorio
        int material_id = random_int(&state,0,5);

        spheres[i] = (Sphere) {
            vec3(x,y,z),
            radius,
            material_id
        };
    }

    // Mostrar información de la escena generada
    printf("\n--- ESCENA ALEATORIA GENERADA ---\n");

    for (int i = 0; i < NUM_SPHERES; i++) {

        printf(
            "Esfera %d: posicion (%.2f, %.2f, %.2f), "
            "radio %.2f, material %d\n",
            i+1,
            spheres[i].center.x,
            spheres[i].center.y,
            spheres[i].center.z,
            spheres[i].radius,
            spheres[i].material_id
        );
    }

    printf(
        "Luz: posicion (%.2f, %.2f, %.2f)\n",
        light.position.x,
        light.position.y,
        light.position.z
    );
}

//  BUSCAR LA INTERSECCION MAS CERCANA

int closest_hit(
    Ray ray,
    double t_min,
    double t_max,
    HitRecord *record
) {
    HitRecord temp;

    int found = 0;
    double closest = t_max;

    for (int i = 0; i < sphere_count; i++) {

        if (hit_sphere(
            ray,
            spheres[i],
            t_min,
            closest,
            &temp
        )) {
            found = 1;
            closest = temp.t;
            *record = temp;
        }
    }

    if (hit_plane(
        ray,
        ground,
        t_min,
        closest,
        &temp
    )) {
        found = 1;
        closest = temp.t;
        *record = temp;
    }

    return found;
}

// COMPROBAR SOMBRAS

int is_in_shadow(Vec3 point, Vec3 normal) {

    Vec3 to_light = sub(light.position,point);

    double distance = length_vec(to_light);

    Vec3 direction = normalize(to_light);

    Ray shadow_ray = make_ray(
        add(point,mul(normal,EPSILON)),
        direction
    );

    HitRecord record;

    return closest_hit(
        shadow_ray,
        EPSILON,
        distance-EPSILON,
        &record
    );
}

// CIELO

Vec3 background_color(Ray ray) {

    double t = 0.5*(ray.direction.y+1.0);

    Vec3 bottom = vec3(0.95,0.95,0.95);
    Vec3 top = vec3(0.35,0.55,0.85);

    return add(
        mul(bottom,1.0-t),
        mul(top,t)
    );
}

// FUNCION PRINCIPAL DE RAY TRACING

Vec3 ray_color(Ray ray, int depth) {

    if (depth <= 0)
        return vec3(0,0,0);

    HitRecord hit;

    if (!closest_hit(
        ray,
        EPSILON,
        1e30,
        &hit
    )) {
        return background_color(ray);
    }

    Material material =
        materials[hit.material_id];

    // Iluminacion ambiental
    Vec3 ambient = mul(material.color,0.12);

    // Direccion hacia la luz
    Vec3 to_light = sub(
        light.position,
        hit.point
    );

    double distance = length_vec(to_light);

    Vec3 L = normalize(to_light);

    // Lambert
    double diffuse_factor = fmax(
        0.0,
        dot(hit.normal,L)
    );

    Vec3 diffuse = vec3(0,0,0);

    // Comprobacion de sombras
    if (diffuse_factor > 0 &&
        !is_in_shadow(hit.point,hit.normal)) {

        double attenuation =
            1.0/(1.0+0.015*distance*distance);

        diffuse = mul(
            hadamard(
                material.color,
                light.intensity
            ),
            diffuse_factor*attenuation
        );
    }

    Vec3 local_color = add(ambient,diffuse);

    // Reflexion recursiva
    double kr = material.reflectivity;

    if (kr > 0.001 && depth > 1) {

        Vec3 reflected_direction = normalize(
            reflect(ray.direction,hit.normal)
        );

        Ray reflected_ray = make_ray(
            add(hit.point,mul(hit.normal,EPSILON)),
            reflected_direction
        );

        Vec3 reflected_color = ray_color(
            reflected_ray,
            depth-1
        );

        local_color = add(
            mul(local_color,1.0-kr),
            mul(reflected_color,kr)
        );
    }

    return clamp_color(local_color);
}

// CAMARA

Camera create_camera(
    Vec3 position,
    Vec3 target,
    Vec3 world_up,
    double fov,
    double aspect
) {
    Camera camera;

    camera.position = position;

    camera.forward = normalize(
        sub(target,position)
    );

    camera.right = normalize(
        cross(camera.forward,world_up)
    );

    camera.up = normalize(
        cross(camera.right,camera.forward)
    );

    double theta = fov*PI/180.0;

    camera.viewport_height =
        2.0*tan(theta/2.0);

    camera.viewport_width =
        aspect*camera.viewport_height;

    return camera;
}

Ray camera_ray(
    Camera camera,
    double u,
    double v
) {
    Vec3 direction = add(
        camera.forward,
        add(
            mul(camera.right,
                (u-0.5)*camera.viewport_width),
            mul(camera.up,
                (v-0.5)*camera.viewport_height)
        )
    );

    return make_ray(
        camera.position,
        direction
    );
}

// CALCULAR UN PIXEL

Vec3 render_pixel(
    int x,
    int y,
    Camera camera
) {
    Vec3 accumulated = vec3(0,0,0);

    uint32_t state = hash32(
        (uint32_t)(x+1)*73856093u ^
        (uint32_t)(y+1)*19349663u
    );

    for (int s = 0; s < SAMPLES; s++) {

        double offset_x = random01(&state);
        double offset_y = random01(&state);

        double u = (
            x+offset_x
        )/(double)WIDTH;

        double v = 1.0-(
            y+offset_y
        )/(double)HEIGHT;

        Ray ray = camera_ray(
            camera,u,v
        );

        Vec3 color = ray_color(
            ray,MAX_DEPTH
        );

        accumulated = add(
            accumulated,color
        );
    }

    return mul(
        accumulated,
        1.0/SAMPLES
    );
}

//  GUARDAR IMAGEN PPM

unsigned char to_byte(double value) {

    value = fmax(0.0,fmin(1.0,value));

    // Correccion gamma
    value = pow(value,1.0/2.2);

    return (unsigned char)(255.999*value);
}

void save_ppm(
    const char *filename,
    Vec3 *framebuffer
) {
    FILE *file = fopen(filename,"wb");

    if (!file) {
        printf("Error creando archivo PPM.\n");
        return;
    }

    fprintf(
        file,
        "P6\n%d %d\n255\n",
        WIDTH,
        HEIGHT
    );

    for (int i = 0; i < WIDTH*HEIGHT; i++) {

        unsigned char pixel[3];

        pixel[0] = to_byte(framebuffer[i].x);
        pixel[1] = to_byte(framebuffer[i].y);
        pixel[2] = to_byte(framebuffer[i].z);

        fwrite(pixel,1,3,file);
    }

    fclose(file);
}

// PROGRAMA PRINCIPAL SERIAL

int main(int argc, char *argv[]) {

    uint32_t seed;

    // OBTENER SEMILLA

    if (argc >= 2) {

        char *endptr;

        errno = 0;

        unsigned long value = strtoul(
            argv[1],
            &endptr,
            10
        );

        if (errno != 0 ||
            endptr == argv[1] ||
            *endptr != '\0') {

            printf("Semilla no valida.\n");
            return 1;
        }

        seed = (uint32_t)value;
    }
    else {

        // Generar semilla a partir de la hora actual
        struct timespec ts;

        timespec_get(&ts,TIME_UTC);

        seed = (uint32_t)(
            (uint64_t)ts.tv_sec ^
            (uint64_t)ts.tv_nsec
        );
    }

    // INFORMACION INICIAL

    printf("====================================\n");
    printf("       RAY TRACER SERIAL\n");
    printf("====================================\n");

    printf("Semilla: %u\n",seed);
    printf("Resolucion: %d x %d\n",WIDTH,HEIGHT);
    printf("Muestras por pixel: %d\n",SAMPLES);
    printf("Profundidad maxima: %d\n",MAX_DEPTH);

    // GENERAR ESCENA ALEATORIA

    initialize_scene(seed);

    printf("\nCantidad de esferas: %d\n",sphere_count);

    // CAMARA

    Camera camera = create_camera(
        vec3(0,3.0,12.0),
        vec3(0,0,-5.0),
        vec3(0,1,0),
        60.0,
        (double)WIDTH/HEIGHT
    );

    // RESERVAR MEMORIA

    Vec3 *framebuffer = malloc(
        (size_t)WIDTH*HEIGHT*sizeof(Vec3)
    );

    if (!framebuffer) {

        printf("Error reservando memoria.\n");

        return 1;
    }

    // INICIO DE MEDICION

    clock_t start = clock();

    // BUCLE SERIAL

    for (int y = 0; y < HEIGHT; y++) {

        for (int x = 0; x < WIDTH; x++) {

            int index = y*WIDTH+x;

            framebuffer[index] = render_pixel(
                x,y,camera
            );
        }
    }

    // FIN DE MEDICION

    clock_t end = clock();

    double elapsed =
        (double)(end-start)/CLOCKS_PER_SEC;

    printf("\nRenderizado terminado.\n");

    printf("Tiempo serial: %.4f segundos\n",elapsed);

    // GUARDAR IMAGEN

    char filename[100];

    snprintf(
        filename,
        sizeof(filename),
        "raytracer_seed_%u.ppm",
        seed
    );

    save_ppm(filename,framebuffer);

    printf("Imagen guardada: %s\n",filename);

    free(framebuffer);

    return 0;
}

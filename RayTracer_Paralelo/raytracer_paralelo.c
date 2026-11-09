
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <omp.h>   // OpenMP: compilar con -fopenmp

// Parametros por defecto = los mismos de la version serial.
// Se pueden cambiar al compilar (util para la Fase 4), por ejemplo:
//   gcc -O2 -fopenmp -DWIDTH=1920 -DHEIGHT=1080 -DSAMPLES=16 ...
#ifndef WIDTH
#define WIDTH 1280
#endif
#ifndef HEIGHT
#define HEIGHT 720
#endif
#ifndef SAMPLES
#define SAMPLES 32
#endif
#ifndef MAX_DEPTH
#define MAX_DEPTH 3
#endif

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

// UTILIDADES PARA LA VERSION PARALELA

// Modo de paralelizacion del bucle
#define MODO_FILAS    0   // #pragma omp for sobre y (cada tarea = 1 fila)
#define MODO_COLLAPSE 1   // #pragma omp for collapse(2) sobre (y, x)

// Convierte el texto del schedule a la constante de OpenMP
static int parse_schedule(const char *name, omp_sched_t *kind) {

    if (strcmp(name, "static") == 0)  { *kind = omp_sched_static;  return 1; }
    if (strcmp(name, "dynamic") == 0) { *kind = omp_sched_dynamic; return 1; }
    if (strcmp(name, "guided") == 0)  { *kind = omp_sched_guided;  return 1; }

    return 0;
}

// Convierte un argumento a entero positivo. Devuelve 1 si es valido.
static int parse_int(const char *text, int min, long *out) {

    char *endptr;

    errno = 0;

    long value = strtol(text, &endptr, 10);

    if (errno != 0 || endptr == text || *endptr != '\0' || value < min)
        return 0;

    *out = value;

    return 1;
}

// Suma de verificacion (FNV-1a) de los bytes de la imagen.
// Sirve para comparar rapido con la version serial sin abrir el archivo.
static uint64_t image_checksum(const Vec3 *framebuffer) {

    uint64_t hash = 1469598103934665603ULL;

    for (int i = 0; i < WIDTH*HEIGHT; i++) {

        unsigned char pixel[3];

        pixel[0] = to_byte(framebuffer[i].x);
        pixel[1] = to_byte(framebuffer[i].y);
        pixel[2] = to_byte(framebuffer[i].z);

        for (int k = 0; k < 3; k++) {
            hash ^= pixel[k];
            hash *= 1099511628211ULL;
        }
    }

    return hash;
}

// PROGRAMA PRINCIPAL PARALELO (OpenMP)
//
// Uso:
//   paralelo.exe [semilla] [hilos] [schedule] [chunk] [modo] [guardar]
//
//   semilla   entero (por defecto: la hora actual)
//   hilos     numero de hilos (por defecto: todos los logicos)
//   schedule  static | dynamic | guided     (por defecto: dynamic)
//   chunk     tamano del bloque; 0 = el de OpenMP (por defecto: 1)
//   modo      filas | collapse              (por defecto: filas)
//   guardar   si | no                       (por defecto: si)

int main(int argc, char *argv[]) {

    uint32_t seed;

    // OBTENER SEMILLA (igual que en la version serial)

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

    // CONFIGURACION OPENMP (argumentos 2 a 6)

    long threads = omp_get_num_procs();
    omp_sched_t sched_kind = omp_sched_dynamic;
    const char *sched_name = "dynamic";
    long chunk = 1;
    int mode = MODO_FILAS;
    int save_image = 1;

    if (argc >= 3 && !parse_int(argv[2], 1, &threads)) {
        printf("Numero de hilos no valido: %s\n", argv[2]);
        return 1;
    }

    if (argc >= 4) {

        if (!parse_schedule(argv[3], &sched_kind)) {
            printf("Schedule no valido: %s "
                   "(use static, dynamic o guided)\n", argv[3]);
            return 1;
        }

        sched_name = argv[3];
    }

    if (argc >= 5 && !parse_int(argv[4], 0, &chunk)) {
        printf("Chunk no valido: %s\n", argv[4]);
        return 1;
    }

    if (argc >= 6) {

        if (strcmp(argv[5], "filas") == 0)
            mode = MODO_FILAS;
        else if (strcmp(argv[5], "collapse") == 0)
            mode = MODO_COLLAPSE;
        else {
            printf("Modo no valido: %s "
                   "(use filas o collapse)\n", argv[5]);
            return 1;
        }
    }

    if (argc >= 7) {

        if (strcmp(argv[6], "no") == 0)
            save_image = 0;
        else if (strcmp(argv[6], "si") != 0) {
            printf("Valor de guardar no valido: %s "
                   "(use si o no)\n", argv[6]);
            return 1;
        }
    }

    // El schedule se elige en ejecucion: el bucle usa schedule(runtime)
    omp_set_dynamic(0);                       // respetar el numero de hilos pedido
    omp_set_num_threads((int)threads);
    omp_set_schedule(sched_kind, (int)chunk); // chunk 0 = valor por defecto

    // INFORMACION INICIAL

    printf("====================================\n");
    printf("   RAY TRACER PARALELO (OpenMP)\n");
    printf("====================================\n");

    printf("Semilla: %u\n",seed);
    printf("Resolucion: %d x %d\n",WIDTH,HEIGHT);
    printf("Muestras por pixel: %d\n",SAMPLES);
    printf("Profundidad maxima: %d\n",MAX_DEPTH);
    printf("Hilos: %ld (nucleos logicos disponibles: %d)\n",
           threads, omp_get_num_procs());
    printf("Schedule: %s, chunk %ld\n", sched_name, chunk);
    printf("Modo: %s\n", mode == MODO_FILAS ? "filas" : "collapse(2)");
    printf("Version de OpenMP (_OPENMP): %d\n", _OPENMP);

    // GENERAR ESCENA ALEATORIA (en serie, antes de la region paralela)

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

    // Estadisticas por hilo (para medir el balance de carga)
    double *thread_time = calloc((size_t)threads, sizeof(double));
    long *thread_pixels = calloc((size_t)threads, sizeof(long));

    if (!framebuffer || !thread_time || !thread_pixels) {

        printf("Error reservando memoria.\n");

        return 1;
    }

    // INICIO DE MEDICION

    double start = omp_get_wtime();

    // BUCLE PARALELO
    //
    // - La escena, la camara y las constantes solo se LEEN: son compartidas.
    // - render_pixel solo usa variables locales y un generador que depende
    //   unicamente de (x, y): no hay estado compartido, no hay carreras.
    // - Cada pixel escribe en su propia casilla del framebuffer.
    // - "nowait" permite que cada hilo anote cuando termino su trabajo.

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        double t0 = omp_get_wtime();
        long pixels_done = 0;

        if (mode == MODO_FILAS) {

            // Cada tarea es una fila completa (aglomeracion de PCAM)
            #pragma omp for schedule(runtime) nowait
            for (int y = 0; y < HEIGHT; y++) {

                for (int x = 0; x < WIDTH; x++) {

                    framebuffer[y*WIDTH+x] = render_pixel(
                        x,y,camera
                    );
                }

                pixels_done += WIDTH;
            }
        }
        else {

            // Cada tarea es un pixel (o un bloque de pixeles seguidos)
            #pragma omp for collapse(2) schedule(runtime) nowait
            for (int y = 0; y < HEIGHT; y++) {

                for (int x = 0; x < WIDTH; x++) {

                    framebuffer[y*WIDTH+x] = render_pixel(
                        x,y,camera
                    );

                    pixels_done++;
                }
            }
        }

        thread_time[tid] = omp_get_wtime() - t0;
        thread_pixels[tid] = pixels_done;
    }   // barrera implicita: aqui terminan todos los hilos

    // FIN DE MEDICION

    double elapsed = omp_get_wtime() - start;

    printf("\nRenderizado terminado.\n");

    printf("Tiempo paralelo: %.4f segundos\n",elapsed);

    // BALANCE DE CARGA POR HILO

    double t_max = 0.0, t_sum = 0.0;

    printf("\n--- TRABAJO POR HILO ---\n");

    for (long i = 0; i < threads; i++) {

        printf("Hilo %2ld: %9ld pixeles, termino a los %.4f s\n",
               i, thread_pixels[i], thread_time[i]);

        if (thread_time[i] > t_max) t_max = thread_time[i];
        t_sum += thread_time[i];
    }

    double t_mean = t_sum/(double)threads;
    double imbalance = (t_mean > 0.0) ? t_max/t_mean : 1.0;

    printf("Desbalance (hilo mas lento / promedio): %.3f\n", imbalance);

    // VERIFICACION

    uint64_t checksum = image_checksum(framebuffer);

    printf("Suma de verificacion de la imagen: %016llx\n",
           (unsigned long long)checksum);

    // Linea para procesar con scripts (Fase 4):
    // semilla,hilos,schedule,chunk,modo,ancho,alto,muestras,tiempo,desbalance
    printf("RESULTADO,%u,%ld,%s,%ld,%s,%d,%d,%d,%.6f,%.4f\n",
           seed, threads, sched_name, chunk,
           mode == MODO_FILAS ? "filas" : "collapse",
           WIDTH, HEIGHT, SAMPLES, elapsed, imbalance);

    // GUARDAR IMAGEN (fuera de la medicion de tiempo)

    if (save_image) {

        char filename[100];

        snprintf(
            filename,
            sizeof(filename),
            "raytracer_paralelo_seed_%u.ppm",
            seed
        );

        save_ppm(filename,framebuffer);

        printf("Imagen guardada: %s\n",filename);
    }

    free(framebuffer);
    free(thread_time);
    free(thread_pixels);

    return 0;
}

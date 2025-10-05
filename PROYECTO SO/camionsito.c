#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define VEHICULOS_POR_HORA 500
#define HORAS_SIMULACION 24
#define TOTAL_VEHICULOS (VEHICULOS_POR_HORA * HORAS_SIMULACION)

// Estructuras de datos
typedef enum { AUTO, CAMION } vehicleType;
typedef enum { DIR_1A4, DIR_4A1 } Direccion;

typedef struct {
    int id;              // Identificación del vehículo
    vehicleType tipo;
    Direccion dir;
    time_t horaEntrada;  // Hora de creación del vehículo
} Vehiculo;

typedef struct {
    sem_t semaforo;
    int capacidad;
    int vehiculosPresentes;
    int contadorAutos;
    int contadorCamiones;
    pthread_mutex_t mutex;
} Subtramo;

typedef struct {
    sem_t semaforo;
    int vehiculosEsperando;
    int maxEspera;
    time_t tiempoMaxEspera;
    time_t tiempoTotalEspera;
    int totalVehiculosEsperado;
    pthread_mutex_t mutex;
} Hombrillo;

// Variables globales
Subtramo subtramos[4];
Hombrillo hombrillos[3];

// Estadísticas
int estadisticasHorarias[24][2] = {0}; // [hora][direccion] - 0:DIR_1A4, 1:DIR_4A1
int estadisticasSubtramos[4][2] = {0}; // [subtramo][direccion]
int totalVehiculosDia = 0;
time_t inicioSimulacion;
pthread_mutex_t statsMutex = PTHREAD_MUTEX_INITIALIZER;

// Inicialización de recursos
void inicializar_recursos()
{
    // Configurar capacidades de subtramos
    subtramos[0].capacidad = 4;  // Subtramo 1
    subtramos[1].capacidad = 2;  // Subtramo 2 (2 autos o 1 camion)
    subtramos[2].capacidad = 1;  // Subtramo 3
    subtramos[3].capacidad = 3;  // Subtramo 4
    
    for (int i = 0; i < 4; i++) {
        sem_init(&subtramos[i].semaforo, 0, subtramos[i].capacidad);
        pthread_mutex_init(&subtramos[i].mutex, NULL);
        subtramos[i].vehiculosPresentes = 0;
        subtramos[i].contadorAutos = 0;
        subtramos[i].contadorCamiones = 0;
    }
    
    for (int i = 0; i < 3; i++) {
        sem_init(&hombrillos[i].semaforo, 0, 999); // Capacidad ilimitada
        pthread_mutex_init(&hombrillos[i].mutex, NULL);
        hombrillos[i].vehiculosEsperando = 0;
        hombrillos[i].maxEspera = 0;
        hombrillos[i].tiempoMaxEspera = 0;
        hombrillos[i].tiempoTotalEspera = 0;
        hombrillos[i].totalVehiculosEsperado = 0;
    }
}

// Función para verificar si puede entrar al subtramo 2
int puede_entrar_subtramo2(vehicleType tipo)
{
    pthread_mutex_lock(&subtramos[1].mutex);
    
    int puede_entrar = 0;
    if (tipo == AUTO)
    {
        puede_entrar = (subtramos[1].contadorAutos < 2);
        printf("🔍 Verificación Subtramo 2 - Autos: %d/%d - %s\n", 
               subtramos[1].contadorAutos, 2, puede_entrar ? "Puede entrar" : "LLENO");
    } else { // CAMION
        puede_entrar = (subtramos[1].contadorCamiones == 0 && subtramos[1].contadorAutos == 0);
        printf("🔍 Verificación Subtramo 2 - Camiones: %d, Autos: %d - %s\n", 
               subtramos[1].contadorCamiones, subtramos[1].contadorAutos, 
               puede_entrar ? "Puede entrar" : "LLENO");
    }
    
    pthread_mutex_unlock(&subtramos[1].mutex);
    return puede_entrar;
}

// Obtener la hora actual de simulación (0-23)
int obtener_hora_actual()
{
    time_t ahora = time(NULL);
    return (int)((ahora - inicioSimulacion) / 3600) % 24;
}

// Actualizar estadísticas horarias
void actualizar_estadisticas_horarias(Direccion dir)
{
    int hora = obtener_hora_actual();
    if (hora >= 0 && hora < 24) {
        pthread_mutex_lock(&statsMutex);
        if (dir == DIR_1A4) {
            estadisticasHorarias[hora][0]++;
        } else {
            estadisticasHorarias[hora][1]++;
        }
        totalVehiculosDia++;
        pthread_mutex_unlock(&statsMutex);
    }
}

// Función principal del vehículo
void* vehiculoThread(void* arg)
{
    Vehiculo* v = (Vehiculo*)arg;
    int inicio, fin, paso;
    
    // Determinar dirección del viaje
    if (v->dir == DIR_1A4)
    {
        inicio = 0; fin = 3; paso = 1;
        printf("🟢 Vehículo %d (%s) INICIANDO viaje dirección 1→4\n", 
               v->id, (v->tipo == AUTO) ? "Auto" : "Camión");
    } else{
        inicio = 3; fin = 0; paso = -1;
        printf("🔵 Vehículo %d (%s) INICIANDO viaje dirección 4→1\n", 
               v->id, (v->tipo == AUTO) ? "Auto" : "Camión");
    }
    
    // Actualizar estadísticas horarias
    actualizar_estadisticas_horarias(v->dir);
    
    // El vehículo comienza en el primer subtramo
    printf("➡️  Vehículo %d entrando DIRECTAMENTE al subtramo %d\n", v->id, inicio + 1);
    sem_wait(&subtramos[inicio].semaforo);
    
    // Actualizar contadores del primer subtramo
    pthread_mutex_lock(&subtramos[inicio].mutex);
    subtramos[inicio].vehiculosPresentes++;
    if (v->tipo == AUTO) {
        subtramos[inicio].contadorAutos++;
    } else {
        subtramos[inicio].contadorCamiones++;
    }
    estadisticasSubtramos[inicio][v->dir]++;
    printf("📊 Subtramo %d - Autos: %d, Camiones: %d, Total: %d\n", 
           inicio + 1, subtramos[inicio].contadorAutos, 
           subtramos[inicio].contadorCamiones, subtramos[inicio].vehiculosPresentes);
    pthread_mutex_unlock(&subtramos[inicio].mutex);
    
    // Recorrer todos los subtramos
    for (int i = inicio; i != fin + paso; i += paso) {
        int siguiente = i + paso;  // El siguiente subtramo
        
        // Verificar si es el último subtramo
        if (siguiente == fin + paso) {
            printf("🎉 Vehículo %d COMPLETÓ su viaje en subtramo %d\n", v->id, i + 1);
            
            // Salir del subtramo actual antes de terminar
            pthread_mutex_lock(&subtramos[i].mutex);
            subtramos[i].vehiculosPresentes--;
            if (v->tipo == AUTO) {
                subtramos[i].contadorAutos--;
            } else {
                subtramos[i].contadorCamiones--;
            }
            pthread_mutex_unlock(&subtramos[i].mutex);
            sem_post(&subtramos[i].semaforo);
            
            break;
        }
        
        // Simular tiempo en el subtramo actual
        int tiempo_subtramo = (rand() % 2) + 1; // 1-2 segundos
        printf("🚗 Vehículo %d CIRCULANDO en subtramo %d (%d segundos)\n", 
               v->id, i + 1, tiempo_subtramo);
        sleep(tiempo_subtramo);
        
        // Salir del subtramo actual
        pthread_mutex_lock(&subtramos[i].mutex);
        subtramos[i].vehiculosPresentes--;
        if (v->tipo == AUTO) {
            subtramos[i].contadorAutos--;
        } else {
            subtramos[i].contadorCamiones--;
        }
        printf("📊 Subtramo %d - Autos: %d, Camiones: %d, Total: %d\n", 
               i + 1, subtramos[i].contadorAutos, 
               subtramos[i].contadorCamiones, subtramos[i].vehiculosPresentes);
        pthread_mutex_unlock(&subtramos[i].mutex);
        
        sem_post(&subtramos[i].semaforo);
        printf("✅ Vehículo %d SALIÓ del subtramo %d\n", v->id, i + 1);
        
        // Calcular índice del hombrillo
        int hombrillo_idx;
        if (paso > 0) { // Dirección 1→4
            hombrillo_idx = i;  // Hombrillo entre i e i+1
        } else { // Dirección 4→1
            hombrillo_idx = i - 1;  // Hombrillo entre i-1 e i
        }
        
        // VERIFICAR si el SIGUIENTE subtramo está lleno - CORREGIDO
        int siguiente_lleno = 0;
        
        if (siguiente == 1) { // Subtramo 2 - verificación especial
            siguiente_lleno = !puede_entrar_subtramo2(v->tipo);
        } else {
            // Para otros subtramos, verificar capacidad normal
            pthread_mutex_lock(&subtramos[siguiente].mutex);
            siguiente_lleno = (subtramos[siguiente].vehiculosPresentes >= subtramos[siguiente].capacidad);
            pthread_mutex_unlock(&subtramos[siguiente].mutex);
        }
        
        if (siguiente_lleno) {
            // IR AL HOMBRILLO a esperar
            printf("🟡 Vehículo %d → Subtramo %d LLENO, YENDO al hombrillo %d\n", 
                   v->id, siguiente + 1, hombrillo_idx + 1);
            
            time_t inicio_espera = time(NULL);
            
            // Entrar al hombrillo
            pthread_mutex_lock(&hombrillos[hombrillo_idx].mutex);
            hombrillos[hombrillo_idx].vehiculosEsperando++;
            if (hombrillos[hombrillo_idx].vehiculosEsperando > hombrillos[hombrillo_idx].maxEspera) {
                hombrillos[hombrillo_idx].maxEspera = hombrillos[hombrillo_idx].vehiculosEsperando;
            }
            pthread_mutex_unlock(&hombrillos[hombrillo_idx].mutex);
            
            // Esperar activamente hasta que haya espacio - CORREGIDO
            int puede_avanzar = 0;
            while (!puede_avanzar)
            {
                usleep(100000);  // Verificar cada 0.1 segundos
                
                if (siguiente == 1) { // Subtramo 2
                    puede_avanzar = puede_entrar_subtramo2(v->tipo);
                } else {
                    // Para otros subtramos
                    pthread_mutex_lock(&subtramos[siguiente].mutex);
                    puede_avanzar = (subtramos[siguiente].vehiculosPresentes < subtramos[siguiente].capacidad);
                    pthread_mutex_unlock(&subtramos[siguiente].mutex);
                }
            }
            
            // Salir del hombrillo
            pthread_mutex_lock(&hombrillos[hombrillo_idx].mutex);
            hombrillos[hombrillo_idx].vehiculosEsperando--;
            pthread_mutex_unlock(&hombrillos[hombrillo_idx].mutex);
            
            time_t fin_espera = time(NULL);
            time_t duracion_espera = fin_espera - inicio_espera;
            
            // Actualizar estadísticas de tiempo de espera
            pthread_mutex_lock(&hombrillos[hombrillo_idx].mutex);
            if (duracion_espera > hombrillos[hombrillo_idx].tiempoMaxEspera) {
                hombrillos[hombrillo_idx].tiempoMaxEspera = duracion_espera;
            }
            hombrillos[hombrillo_idx].tiempoTotalEspera += duracion_espera;
            hombrillos[hombrillo_idx].totalVehiculosEsperado++;
            pthread_mutex_unlock(&hombrillos[hombrillo_idx].mutex);
            
            printf("⏱️  Vehículo %d ESPERÓ %ld segundos en hombrillo %d\n", 
                   v->id, duracion_espera, hombrillo_idx + 1);
        } else {
            printf("🟢 Vehículo %d → Subtramo %d tiene ESPACIO, AVANZANDO directamente\n", 
                   v->id, siguiente + 1);
        }
        
        // ENTRAR al siguiente subtramo
        printf("➡️  Vehículo %d ENTRANDO al subtramo %d\n", v->id, siguiente + 1);
        sem_wait(&subtramos[siguiente].semaforo);
        
        // Actualizar contadores del subtramo - CORREGIDO
        pthread_mutex_lock(&subtramos[siguiente].mutex);
        subtramos[siguiente].vehiculosPresentes++;
        if (v->tipo == AUTO) {
            subtramos[siguiente].contadorAutos++;
        } else {
            subtramos[siguiente].contadorCamiones++;
        }
        estadisticasSubtramos[siguiente][v->dir]++;
        printf("📊 Subtramo %d - Autos: %d, Camiones: %d, Total: %d\n", 
               siguiente + 1, subtramos[siguiente].contadorAutos, 
               subtramos[siguiente].contadorCamiones, subtramos[siguiente].vehiculosPresentes);
        pthread_mutex_unlock(&subtramos[siguiente].mutex);
    }
    
    printf("🏁 Vehículo %d terminó su recorrido\n", v->id);
    free(v);
    return NULL;
}

// Función para mostrar estadísticas
void mostrar_estadisticas()
{
    printf("\n");
    printf("📊 ========== ESTADÍSTICAS FINALES ==========\n");
    
    // Estadísticas horarias
    printf("\n📈 ESTADÍSTICAS HORARIAS (vehículos generados por hora):\n");
    printf("Hora | Dirección 1→4 | Dirección 4→1 | Total\n");
    printf("-----|---------------|---------------|-------\n");
    for (int hora = 0; hora < 24; hora++) {
        int total_hora = estadisticasHorarias[hora][0] + estadisticasHorarias[hora][1];
        printf("%2d   | %13d | %13d | %5d\n", 
               hora, estadisticasHorarias[hora][0], estadisticasHorarias[hora][1], total_hora);
    }
    
    // Estadísticas por subtramo
    printf("\n🛣️  ESTADÍSTICAS POR SUBTRAMO (vehículos que circularon):\n");
    for (int i = 0; i < 4; i++) {
        int total_subtramo = estadisticasSubtramos[i][0] + estadisticasSubtramos[i][1];
        printf("Subtramo %d:\n", i + 1);
        printf("  Dirección 1→4: %d vehículos\n", estadisticasSubtramos[i][0]);
        printf("  Dirección 4→1: %d vehículos\n", estadisticasSubtramos[i][1]);
        printf("  Total: %d vehículos\n", total_subtramo);
    }
    
    // Estadísticas de hombrillos
    printf("\n🅿️  ESTADÍSTICAS DE HOMBRILLOS:\n");
    for (int i = 0; i < 3; i++) {
        printf("Hombrillo %d-%d:\n", i + 1, i + 2);
        printf("  Máximo vehículos esperando: %d\n", hombrillos[i].maxEspera);
        printf("  Tiempo máximo de espera: %ld segundos\n", hombrillos[i].tiempoMaxEspera);
        if (hombrillos[i].totalVehiculosEsperado > 0) {
            double promedio = (double)hombrillos[i].tiempoTotalEspera / hombrillos[i].totalVehiculosEsperado;
            printf("  Tiempo promedio de espera: %.2f segundos\n", promedio);
        }
        printf("  Total vehículos que esperaron: %d\n", hombrillos[i].totalVehiculosEsperado);
    }
    
    printf("\n📦 TOTAL DE VEHÍCULOS EN EL DÍA: %d\n", totalVehiculosDia);
    printf("==========================================\n");
}

// Función para limpiar recursos
void limpiar_recursos()
{
    for (int i = 0; i < 4; i++) {
        sem_destroy(&subtramos[i].semaforo);
        pthread_mutex_destroy(&subtramos[i].mutex);
    }
    
    for (int i = 0; i < 3; i++) {
        sem_destroy(&hombrillos[i].semaforo);
        pthread_mutex_destroy(&hombrillos[i].mutex);
    }
    
    pthread_mutex_destroy(&statsMutex);
}

int main() {
    srand(time(NULL));
    inicioSimulacion = time(NULL);
    inicializar_recursos();

    printf("🚦 INICIANDO SIMULACIÓN DE TRÁFICO\n");
    printf("⏰ Duración: %d horas\n", HORAS_SIMULACION);
    printf("🚗 Vehículos por hora: %d\n", VEHICULOS_POR_HORA);
    printf("📊 Total de vehículos: %d\n", TOTAL_VEHICULOS);
    printf("==========================================\n");

    int vehiculosGenerados = 0;
    
    // Calcular microsegundos entre vehículos para mantener tasa de 500/hora
    int microsegundos_entre_vehiculos = (3600 * 1000000) / VEHICULOS_POR_HORA;
    
    while (vehiculosGenerados < TOTAL_VEHICULOS)
    {
        Vehiculo* v = malloc(sizeof(Vehiculo));
        v->id = vehiculosGenerados + 1;
        v->tipo = (rand() % 4 == 0) ? CAMION : AUTO;
        v->dir = (rand() % 2) ? DIR_1A4 : DIR_4A1;
        v->horaEntrada = time(NULL);
        
        pthread_t hilo;
        pthread_create(&hilo, NULL, vehiculoThread, v);
        pthread_detach(hilo);
        
        vehiculosGenerados++;
        
        // Mostrar progreso cada 100 vehículos
        if (vehiculosGenerados % 100 == 0) {
            printf("📦 Generados %d/%d vehículos\n", vehiculosGenerados, TOTAL_VEHICULOS);
        }
        
        // Esperar tiempo calculado para mantener tasa de 500/hora
        usleep(rand() % (microsegundos_entre_vehiculos * 2));
    }
    
    printf("✅ GENERACIÓN DE VEHÍCULOS COMPLETADA\n");
    printf("⏳ Esperando que terminen los vehículos en circulación...\n");
    
    // Esperar a que terminen los vehículos (simulación simplificada)
    sleep(30);
    
    mostrar_estadisticas();
    limpiar_recursos();
    
    printf("🎯 SIMULACIÓN COMPLETADA EXITOSAMENTE\n");
    return 0;
}
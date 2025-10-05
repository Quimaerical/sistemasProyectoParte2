#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define MAX_VEHICULOS 500
#define HORAS_SIMULACION 24

//Estructuras de datos
typedef enum { AUTO, CAMION } vehicleType;
typedef enum { DIR_1A4, DIR_4A1 } Direccion;

typedef struct {
    int id;              // Identificacion del vehiculo
    vehicleType tipo;
    Direccion dir;
    time_t horaEntrada; // En el hombrillo
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
    pthread_mutex_t mutex;
} Hombrillo;

//Variables globales
Subtramo subtramos[4];
Hombrillo hombrillos[3];

//Inicialización de recursos
void inicializar_recursos()
{
    //Configurar capacidades de subtramos
    subtramos[0].capacidad = 4;  // Subtramo 1 (vehículos de cualquier tipo)
    subtramos[1].capacidad = 2;  // Subtramo 2 (2 autos o 1 camion)
    subtramos[2].capacidad = 1;  // Subtramo 3 (vehículos de cualquier tipo)
    subtramos[3].capacidad = 3;  // Subtramo 4 (vehículos de cualquier tipo)
    
    for (int i = 0; i < 4; i++)
    {
        sem_init(&subtramos[i].semaforo, 0, subtramos[i].capacidad);
        pthread_mutex_init(&subtramos[i].mutex, NULL);
        subtramos[i].vehiculosPresentes = 0;
        subtramos[i].contadorAutos = 0;
        subtramos[i].contadorCamiones = 0;
    }
    
    for (int i = 0; i < 3; i++)
    {
        sem_init(&hombrillos[i].semaforo, 0, 999); //Capacidad del hombrillo
        pthread_mutex_init(&hombrillos[i].mutex, NULL);
        hombrillos[i].vehiculosEsperando = 0;
        hombrillos[i].maxEspera = 0;
    }
}

//Función principal del vehículo (Hilo de un vehiculo)
void* vehiculoThread(void* arg) {
    Vehiculo* v = (Vehiculo*)arg;
    int inicio, fin, paso;
    
}

int main()
{
    srand(time(NULL));  //Semilla random
    inicializar_recursos();

    int vehiculosGenerados = 0;
    
    while (vehiculosGenerados < 4 ) // 4 por test
    {
        Vehiculo* v = malloc(sizeof(Vehiculo));

        v->id = vehiculosGenerados + 1;
        v->tipo = (rand() % 4 == 0) ? CAMION : AUTO; // 25% camiones, 75% autos
        v->dir = (rand() % 2) ? DIR_1A4 : DIR_4A1;   // 50% -> ó <-
        
        pthread_t hilo;
        pthread_create(&hilo, NULL, vehiculoThread, v); //crea e inicia hilo
        pthread_detach(hilo); //No se q hace, Deepseek dice que es necesario
        
        vehiculosGenerados++;
        
        // Esperar tiempo aleatorio entre generación de vehículos
        //usleep(rand() % 1000000); // Entre 0-1 segundo
        //usleep(rand() % 100000);
    }

    

    return 0;
}
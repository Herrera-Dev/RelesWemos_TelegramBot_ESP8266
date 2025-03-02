const String msjLed = "LEDS DE ALERTAS\n+ 🔴 - Sin conexion WiFi.\n+ 🔵 - Sin conexion a internet.\n+ 🟡 - Sin credenciales.\n+ ⚪️ - Modo configuracion.\n+ 🟢 - Sin ningun problema.";
const String msjComan = "\n\n👨‍💻COMANDOS\n/confCreden: Config. credenciales de acceso por WifiManager.\n/bloquear o /desbloq: Bloque el acceso a todos los usuarios.\n/control: Encender o apagar los reles.\n/estado: Estado actual de todos los reles.\n/usuarios: Lista de usuarios registrados.\n/reles: Lista de reles registrados.\n/controlAut: Config. de encendido y apagado automatico.\n/confUsuarios: Config. de los usuarios permitidos.\n/confReles: Config. de los reles registrados.\n/reiniciar: Reiniciar el dispositivo.";
const String msjComanDev = "\n\nCOMANDOS DEV\n/resetDatos: Limpiar los datos.\n/testReles: Probar el funcionamiento de los reles.\n/pruebaLed: Probar los leds de advertencias.\n/debug: Mostrar los datos por el puerto serial.";
const String symbol_run = "▶️  ";
const String symbol_stop = "⏸️  ";
const char *myDomain = "api.telegram.org";
const char *npt = "pool.ntp.org";
const char *hostname = "TelegramBot";
const char *hostpass = "12345678";

const int cantUsuarios = 4;
const int cantAparatos = 4;
const int pines[cantAparatos] = {5, 4, 0, 2}; // {D1, D2, D3, D4}
bool avisosE[] = {false, false, false, false};
bool avisosA[] = {false, false, false, false};

struct config
{
    bool encend;
    bool modConfig;
    int modLed;
    char token[50];
    char msjError[30];
};

struct users
{
    char id[12];
    char nombre[16];
    bool vacio;
    bool activo;
};

struct Reles
{
    int pin;
    char nombre[16];
    bool vacio;
    bool activo;
    bool estado;
    bool controlAut;
    bool contAutEst;
    char hEnc[6];
    char hApag[6];
};
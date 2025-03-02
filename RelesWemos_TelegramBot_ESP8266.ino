
#define resetDats 0 // ELIMINAR LOS DATOS ALMACENANDOS (NO WIFI) -> activar una solo una vez.

#include <WiFiManager.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include "datos.h"

#define btn_AP 15 // btn de configuracion.
#define ledR 14
#define ledG 12
#define ledB 13

__attribute__((section(".noinit"))) int ultimoMsjBot = 0;
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure client;
UniversalTelegramBot bot("", client);

struct tm timeinfo;
byte desc = 0;
byte user;
bool led = false;
unsigned long tAntConsulta;
unsigned long tAntRevis;

int posiUsr = 0, posiRel = 0;
bool modConfigUser = false, modConfigRel = false;
int optionUserSelect = 0, optionReleSelect = 0;
int usrSelect = 0, releSelect = 0;
bool modConfigAut = false;
int posiAut = 0, disp = 0;

users usuarios[cantUsuarios];
Reles aparatos[cantAparatos];
config conf[1];

// ==============================================
void setup()
{
  Serial.begin(115200);
  Serial.println(F("\n\n=========="));
  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledB, OUTPUT);
  pinMode(btn_AP, INPUT);
  for (int i = 0; i < cantAparatos; i++)
  {
    pinMode(pines[i], OUTPUT);
    digitalWrite(pines[i], LOW);
  }

#if resetDats
  resetDatos(); // Reiniciar todoa los datos
#endif
  if (cargarDatos())
  {
    delay(10);
    for (int i = 0; i < cantAparatos; i++) // Configurar pines.
    {
      digitalWrite(pines[i], aparatos[i].estado);
    }
  }

  confConexiones();
  configTime(-14400, 0, npt);
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    delay(100);
    now = time(nullptr);
  }
  delay(1500);

  newMensajes(false);
  for (int i = 0; i < cantUsuarios; i++) // Bienvenida.
  {
    iniciar(i);
  }

  strncpy(conf[0].msjError, " ", sizeof(conf[0].msjError));
  conf[0].modLed = 1;
  guardarConfig();
  tAntConsulta = millis();
  tAntRevis = millis();
}
void loop()
{
  if ((millis() - tAntRevis) > 300000)
  {

    if (WiFi.status() == WL_CONNECTED)
    {
      WiFiClient client;
      if (!client.connect("www.google.com", 80))
      {
        conf[0].modLed = 3;
        if (desc == 4)
        {
          Serial.println(F("Perdida de conexion.\nReiniciando...."));
          strncpy(conf[0].msjError, "Perdida de conexion.", sizeof(conf[0].msjError));
          guardarConfig();
          delay(1000);
          ESP.restart();
        }
        desc++;
      }
      else
      {
        configTime(-14400, 0, npt);
        client.stop();
        desc = 0;
      }
      tAntRevis = millis();
    }
    else
    {
      WiFi.reconnect();
      conf[0].modLed = 2;

      if (desc == 4)
      {
        Serial.println(F("Perdida de WiFi.\nReiniciando...."));
        strncpy(conf[0].msjError, "Perdida de Wifi.", sizeof(conf[0].msjError));
        guardarConfig();
        delay(1000);
        ESP.restart();
      }
      desc++;
    }
  }

  if (conf[0].modLed == 1)
  {
    alertasLed(1);
  }
  else
  {
    led = !led;
    led ? alertasLed(0) : alertasLed(conf[0].modLed);
  }

  controlAuto();
  newMensajes(true);
  delay(500);
}
void telegramBot(int msj)
{
  for (int i = 0; i < msj; i++)
  {
    if (bot.messages[i].message_id == ultimoMsjBot)
    {
      Serial.println("msj ignorado.");
      continue;
    }

    Serial.println(F("\n----------------"));
    Serial.print("new msj: ");
    String chat_id = bot.messages[i].chat_id;
    String chat_name = bot.messages[i].from_name;
    String chat_text = bot.messages[i].text;
    Serial.println(chat_text);
    delay(200);

    user = seguridad(chat_id);
    if (user != cantUsuarios)
    {
      if (chat_text == "/resetDatos" && user == 0)
      {
        Serial.println("LIMPIANDO");
        ultimoMsjBot = bot.messages[0].message_id;
        resetDatos();
        bot.sendMessage(usuarios[user].id, "Datos eliminados de la memoria (no WiFi) 🗑️\nReiniciando...", "");
        continue;
      }
      if (chat_text == "/testReles" && user == 0)
      {
        ultimoMsjBot = bot.messages[0].message_id;
        testReles();
        bot.sendMessage(usuarios[user].id, "Test de reles terminado.", "");
        continue;
      }
      if (chat_text == "/pruebaLed" && user == 0)
      {
        ultimoMsjBot = bot.messages[0].message_id;
        for (int i = 0; i < 5; i++)
        {
          int n = 0;
          while (n < 6)
          {
            alertasLed(n);
            delay(5000);
            n++;
          }
        }
        bot.sendMessage(usuarios[user].id, "Prueba de leds terminado.", "");
      }
      if (chat_text == "/debug" && user == 0)
      {
        ultimoMsjBot = bot.messages[0].message_id;
        debug();
        continue;
      }

      if (!conf[0].encend && user != 0)
      {
        ultimoMsjBot = bot.messages[0].message_id;
        bot.sendMessage(usuarios[user].id, "El ESP8266 esta bloqueado ❌🤖", "");
        continue;
      }

      if (chat_text == "/start" || chat_text == "/ayuda")
      {
        ultimoMsjBot = bot.messages[user].message_id;
        iniciar(user);
        bot.sendMessage(usuarios[user].id, msjLed, "");
        continue;
      }
      if (chat_text == "/ayudaBot")
      {
        ultimoMsjBot = bot.messages[user].message_id;
        bot.sendMessage(usuarios[user].id, (msjLed + msjComan + msjComanDev), "");
        continue;
      }

      if (chat_text == "/cancelar")
      {
        ultimoMsjBot = bot.messages[user].message_id;
        bot.sendMessageWithReplyKeyboard(usuarios[0].id, "Configuracion en proceso cancelado.", "", menuEst(0), true);
        cancelar();
        continue;
      }
      if (chat_text == "/confCreden")
      {
        ultimoMsjBot = bot.messages[user].message_id;
        if (user != 0)
        {
          bot.sendMessage(usuarios[user].id, "Opcion no permitida para el usuario ❌👷‍♂️", "");
          continue;
        }
        conf[0].modLed = 5;
        conf[0].modConfig = true;
        guardarConfig();
        bot.sendMessage(chat_id, "Reiniciando... ⬇️\n\n⚙ Ingresar al AP: http://192.168.4.1 \n- WiFi: " + String(hostname) + "\n- Pass: " + hostpass, "");

        Serial.println("Reiniciando..");
        ESP.restart();
        continue;
      }

      if (chat_text == "/estado") // Estado de los dispositivos y usuario act. o desactiv.
      {
        ultimoMsjBot = bot.messages[user].message_id;
        bot.sendMessage(usuarios[user].id, estado(user), "");
        continue;
      }

      if ((chat_text == "/bloquear" || chat_text == "/desbloq")) // BLOQ. Y DESBLOQ. Para otros usuarios.
      {
        ultimoMsjBot = bot.messages[user].message_id;
        if (user != 0)
        {
          bot.sendMessage(usuarios[user].id, "Opcion no permitida para el usuario ❌👷‍♂️", "");
          continue;
        }

        conf[0].encend = !conf[0].encend;
        String mjs = conf[0].encend ? "Equipo RelesTelegramBot DESBLOQUEADO ✅🤖" : "Equipo RelesTelegramBot BLOQUEADO ❌🤖";

        for (int i = 0; i < cantUsuarios; i++)
        {
          if (!usuarios[i].vacio)
          {
            bot.sendMessageWithReplyKeyboard(usuarios[i].id, mjs, "", menuEst(i), true);
          }
        }
        guardarConfig();
        continue;
      }

      if (chat_text == "/usuarios")
      {
        ultimoMsjBot = bot.messages[user].message_id;
        bot.sendMessage(usuarios[user].id, estadoUsuarios(user), "");
        continue;
      }

      if (chat_text == "/reles")
      {
        ultimoMsjBot = bot.messages[user].message_id;
        bot.sendMessage(usuarios[user].id, estadoReles(), "");
        continue;
      }

      if (chat_text == "/control")
      {
        ultimoMsjBot = bot.messages[user].message_id;
        if (usuarios[user].activo)
        {
          control(chat_id);
        }
        else
        {
          bot.sendMessage(usuarios[user].id, "El usuario esta BLOQUEADO ❌👷‍♂️", "");
        }
        continue;
      }
      if (chat_text.charAt(0) == '#')
      {
        ultimoMsjBot = bot.messages[user].message_id;
        if (usuarios[user].activo)
        {
          cambiarEstado(i);
        }
        else
        {
          bot.sendMessage(usuarios[user].id, "El usuario esta BLOQUEADO ❌👷‍♂️", "");
        }
        continue;
      }

      // OPCION CONTROL AUT
      if (chat_text == "/controlAut")
      {
        ultimoMsjBot = bot.messages[user].message_id;
        if (usuarios[user].activo)
        {
          String text = "[[\"/agregarControlAut\", \"/estadoControlAut\"], [\"/eliminarControlAut\", \"/cancelar\"]]";
          bot.sendMessageWithReplyKeyboard(usuarios[user].id, "CONFIGURAR CONTROL AUTOMATICO ⏳\nSeleccionar una opcion:", "", text, true);
          modConfigAut = true;
        }
        else
        {
          bot.sendMessage(usuarios[user].id, "El usuario esta BLOQUEADO ❌👷‍♂️", "");
        }
        continue;
      }
      if (modConfigAut)
      {
        ultimoMsjBot = bot.messages[user].message_id;
        String x;
        int indiceGuion = -1;
        int indicePunt = -1;

        if (chat_text == "/agregarControlAut")
        {
          x = "[[\"1. " + String(aparatos[0].nombre) + "\", \"2. " + String(aparatos[1].nombre) + "\"], [\"3. " + String(aparatos[2].nombre) + "\", \"4. " + String(aparatos[3].nombre) + "\"], [\"/cancelar\"]]";
          bot.sendMessageWithReplyKeyboard(usuarios[user].id, "Seleccione un dispositivo:", "", x, true);
          posiAut = 1;
          continue;
        }
        if (chat_text == "/estadoControlAut")
        {
          x = "[[\"1. " + String(aparatos[0].nombre) + "\", \"2. " + String(aparatos[1].nombre) + "\"], [\"3. " + String(aparatos[2].nombre) + "\", \"4. " + String(aparatos[3].nombre) + "\"], [\"/cancelar\"]]";
          bot.sendMessageWithReplyKeyboard(usuarios[user].id, "Seleccione un dispositivo:", "", x, true);
          posiAut = 3;
          continue;
        }
        else if (chat_text == "/eliminarControlAut")
        {
          x = "[[\"1. " + String(aparatos[0].nombre) + "\", \"2. " + String(aparatos[1].nombre) + "\"], [\"3. " + String(aparatos[2].nombre) + "\", \"4. " + String(aparatos[3].nombre) + "\"], [\"/cancelar\"]]";
          bot.sendMessageWithReplyKeyboard(usuarios[user].id, "Seleccione un dispositivo:", "", x, true);
          posiAut = 4;
          continue;
        }

        switch (posiAut)
        {
        case 1:
          disp = String(chat_text.charAt(0)).toInt() - 1;
          if (disp < 0 || disp > 3 || aparatos[disp].vacio)
          {
            bot.sendMessage(usuarios[user].id, "Seleccione un dispositivo valido ❌", "");
            continue;
          }
          x = "Ingresar la horas de " + String(aparatos[disp].nombre) + ".\nFORMATO: 17:01-07:00, 10:00-vacio";
          bot.sendMessage(usuarios[user].id, x, "");
          posiAut = 2;
          continue;
          break;

        case 2:
          indiceGuion = chat_text.indexOf('-');
          indicePunt = chat_text.indexOf(':');
          if (indiceGuion != -1 && indicePunt != -1)
          {
            if (aparatos[disp].vacio)
            {
              bot.sendMessage(usuarios[user].id, "No existe el dispositivo ❌", "");
              continue;
            }

            String hEnc = chat_text.substring(0, indiceGuion);
            String hApag = chat_text.substring(indiceGuion + 1);
            aparatos[disp].controlAut = true;
            aparatos[disp].contAutEst = true;
            hEnc = (hEnc == "vacion" || hEnc == "Vacio") ? "N/A" : hEnc;
            hApag = (hApag == "vacion" || hApag == "Vacio") ? "N/A" : hApag;
            hEnc.toCharArray(aparatos[disp].hEnc, sizeof(aparatos[disp].hEnc));
            hApag.toCharArray(aparatos[disp].hApag, sizeof(aparatos[disp].hApag));
            x = "Configuracion automatica de " + String(aparatos[disp].nombre) + " ✅\n";
            x += (hEnc == "N/A") ? ("- Encender: Ninguno.\n") : ("- Encender: " + String(hEnc) + " hrs.\n");
            x += (hApag == "N/A") ? ("- Apagar: Ninguno.\n") : ("- Apagar: " + String(hApag) + " hrs.\n");
            for (int i = 0; i < cantUsuarios; i++)
            {
              if (!usuarios[i].vacio)
              {
                bot.sendMessageWithReplyKeyboard(usuarios[i].id, x, "", menuEst(i), true);
              }
            }
            guardarReles();
            cancelar();
          }
          else
          {
            bot.sendMessage(usuarios[user].id, "No se cumplio en formato ❌✏️", "");
          }
          continue;
          break;

        case 3:
          disp = String(chat_text.charAt(0)).toInt() - 1;
          if (disp < 0 || disp > 3 || !aparatos[disp].controlAut || !aparatos[disp].activo)
          {
            bot.sendMessage(usuarios[user].id, "Seleccione un dispositivo valido ❌", "");
            continue;
          }
          aparatos[disp].contAutEst = !aparatos[disp].contAutEst;
          x = "Control automatico para " + String(aparatos[disp].nombre) + " " + (aparatos[disp].contAutEst ? "ACTIVADO  ✅" : "DESACTIVADO  ✅");
          for (int i = 0; i < cantUsuarios; i++)
          {
            if (!usuarios[i].vacio)
            {
              bot.sendMessageWithReplyKeyboard(usuarios[i].id, x, "", menuEst(i), true);
            }
          }
          guardarReles();
          cancelar();
          continue;
          break;

        case 4:
          disp = String(chat_text.charAt(0)).toInt() - 1;
          if (disp < 0 || disp > 3 || !aparatos[disp].controlAut)
          {
            bot.sendMessage(usuarios[user].id, "Seleccione un dispositivo valido ❌", "");
            continue;
          }
          aparatos[disp].controlAut = false;
          aparatos[disp].contAutEst = false;
          x = "N/A";
          x.toCharArray(aparatos[disp].hEnc, sizeof(aparatos[disp].hEnc));
          x.toCharArray(aparatos[disp].hApag, sizeof(aparatos[disp].hApag));
          x = "Control aut. para " + String(aparatos[disp].nombre) + " eliminado 🗑️✅";
          for (int i = 0; i < cantUsuarios; i++)
          {
            if (!usuarios[i].vacio)
            {
              bot.sendMessageWithReplyKeyboard(usuarios[i].id, x, "", menuEst(i), true);
            }
          }
          guardarReles();
          cancelar();
          continue;
          break;

        default:
          bot.sendMessage(usuarios[user].id, "Comando incorrecto ❌", "");
          continue;
          break;
        }
      }

      // OPCION USUARIOS
      if (chat_text == "/confUsuarios") // 1
      {
        ultimoMsjBot = bot.messages[user].message_id;
        if (user != 0)
        {
          bot.sendMessage(usuarios[user].id, "Opcion no permitida para el usuario ❌👷‍♂️", "");
          continue;
        }

        const String text = "[[\"/agregarUsuarios\", \"/on/offUsuarios\"], [\"/eliminarUsuarios\", \"/cancelar\"]]";
        bot.sendMessageWithReplyKeyboard(usuarios[user].id, "👷‍♂️ CONFIGURACION DE USUARIOS\nSeleccionar una opcion de usuarios:", "", text, true);
        modConfigUser = true;
        posiUsr = 1;
        continue;
      }
      if (modConfigUser)
      {
        ultimoMsjBot = bot.messages[user].message_id;
        if (user != 0)
        {
          bot.sendMessage(usuarios[user].id, "Opcion no permitida para el usuario ❌👷‍♂️", "");
          continue;
        }

        String text;
        int indiceGuion;
        switch (posiUsr)
        {
        case 1:
          if (chat_text == "/agregarUsuarios")
          {
            optionUserSelect = 1;
          }
          else if (chat_text == "/on/offUsuarios")
          {
            optionUserSelect = 2;
          }
          else if (chat_text == "/eliminarUsuarios")
          {
            optionUserSelect = 3;
          }
          else
          {
            bot.sendMessage(usuarios[user].id, "Opcion incorrecta ❌", "");
            continue;
          }

          text = "[[\"1. " + String(usuarios[0].nombre) + "\", \"2. " + String(usuarios[1].nombre) + "\"], [\"3. " + String(usuarios[2].nombre) + "\", \"4. " + String(usuarios[3].nombre) + "\"], [\"/cancelar\"]]";
          bot.sendMessageWithReplyKeyboard(usuarios[user].id, "Seleccione un usuario:", "", text, true);
          posiUsr = 2;
          continue;
          break;
        case 2:
          usrSelect = String(chat_text.charAt(0)).toInt() - 1;
          if (usrSelect < 0 || usrSelect > 3)
          {
            bot.sendMessage(usuarios[user].id, "Seleccione un usuario valido ❌", "");
            continue;
          }

          switch (optionUserSelect)
          {
          case 1:
            bot.sendMessage(usuarios[user].id, "FORMATO: nombre-idUsuario", "");
            posiUsr = 3;
            continue;
            break;

          case 2:
            if (!usuarios[usrSelect].vacio)
            {
              if (usrSelect == 0)
              {
                bot.sendMessage(usuarios[user].id, "Al (adm) no es posible bloquear.", "");
                continue;
              }

              usuarios[usrSelect].activo = !usuarios[usrSelect].activo;
              if (usuarios[usrSelect].activo)
              {
                text = "Usuario " + String(usuarios[usrSelect].nombre) + " HABILITADO ✅";
                bot.sendMessageWithReplyKeyboard(usuarios[user].id, text, "", menuEst(0), true);
                bot.sendMessageWithReplyKeyboard(usuarios[usrSelect].id, text, "", menuEst(usrSelect), true);
              }
              else
              {
                text = "Usuario " + String(usuarios[usrSelect].nombre) + " BLOQUEADO ⛔";
                bot.sendMessageWithReplyKeyboard(usuarios[user].id, text, "", menuEst(0), true);
                bot.sendMessageWithReplyKeyboard(usuarios[usrSelect].id, text, "", menuEst(usrSelect), true);
              }
              guardarUsers();
              cancelar();

              debug();
            }
            else
            {
              bot.sendMessage(usuarios[user].id, "Usuario vacio ❌", "");
            }
            continue;
            break;

          case 3:
            if (!usuarios[usrSelect].vacio)
            {
              usuarios[usrSelect].activo = !usuarios[usrSelect].activo;
              String id = String(usrSelect);
              String txt = "Vacio";
              id.toCharArray(usuarios[usrSelect].id, 11);
              txt.toCharArray(usuarios[usrSelect].nombre, 15);
              usuarios[usrSelect].vacio = true;
              usuarios[usrSelect].activo = false;

              bot.sendMessageWithReplyKeyboard(usuarios[user].id, "Usuario eliminado 🗑️", "", menuEst(user), true);
              bot.sendMessageWithReplyKeyboard(usuarios[usrSelect].id, "Este usuario fue eliminado 🗑️", "", menuEst(usrSelect), true);

              guardarUsers();
              cancelar();

              debug();
            }
            else
            {
              bot.sendMessage(usuarios[user].id, "Usuario vacio ❌", "");
            }
            continue;
            break;

          default:
            bot.sendMessage(usuarios[user].id, "Comando incorrecto ❌", "");
            continue;
            break;
          }
          continue;
          break;
        case 3:
          indiceGuion = chat_text.indexOf('-');
          if (indiceGuion != -1)
          {
            String nom = chat_text.substring(0, indiceGuion);
            String id = chat_text.substring(indiceGuion + 1);
            id.toCharArray(usuarios[usrSelect].id, 11);
            nom.toCharArray(usuarios[usrSelect].nombre, 15);
            usuarios[usrSelect].vacio = false;
            usuarios[usrSelect].activo = true;

            bot.sendMessageWithReplyKeyboard(usuarios[user].id, "Usuario " + nom + " agregado y habilitado 👍", "", menuEst(0), true);
            delay(15);

            bot.sendMessageWithReplyKeyboard(id, "Ya tienes acceso al control de WemosReleBot 🤖👍", "", menuEst(1), true);
            guardarUsers();
            cancelar();

            debug();
          }
          else
          {
            bot.sendMessage(usuarios[user].id, "No se cumplio en formato ❌✏️", "");
          }
          continue;
          break;

        default:
          continue;
          break;
        }
      }

      // OPCION RELES
      if (chat_text == "/confReles") // 1
      {
        ultimoMsjBot = bot.messages[user].message_id;
        if (user != 0)
        {
          bot.sendMessage(usuarios[user].id, "Opcion no permitida para el usuario ❌👷‍♂️", "");
          continue;
        }

        const String text = "[[\"/agregarRele\", \"/on/offRele\"], [\"/eliminarRele\", \"/cancelar\"]]";
        bot.sendMessageWithReplyKeyboard(usuarios[user].id, "🎛 CONFIGURACION DE RELES\nSeleccionar una opcion de reles:", "", text, true);
        modConfigRel = true;
        posiRel = 1;
        continue;
      }
      if (modConfigRel)
      {
        ultimoMsjBot = bot.messages[user].message_id;
        if (user != 0)
        {
          bot.sendMessage(usuarios[user].id, "Opcion no permitida para el usuario ❌👷‍♂️", "");
          continue;
        }

        String text;
        switch (posiRel)
        {
        case 1: // Seleccionar opcion
          if (chat_text == "/agregarRele")
          {
            optionReleSelect = 1;
          }
          else if (chat_text == "/on/offRele")
          {
            optionReleSelect = 2;
          }
          else if (chat_text == "/eliminarRele")
          {
            optionReleSelect = 3;
          }
          else
          {
            bot.sendMessage(usuarios[user].id, "Opcion incorrecta ❌", "");
            continue;
          }

          text = "[[\"1. " + String(aparatos[0].nombre) + "\", \"2. " + String(aparatos[1].nombre) + "\"], [\"3. " + String(aparatos[2].nombre) + "\", \"4. " + String(aparatos[3].nombre) + "\"], [\"/cancelar\"]]";
          bot.sendMessageWithReplyKeyboard(usuarios[user].id, "Seleccione un dispositivo:", "", text, true);
          posiRel = 2;
          continue;
          break;

        case 2: // Seleccionar dispositivo
          releSelect = String(chat_text.charAt(0)).toInt() - 1;
          if (releSelect < 0 || releSelect > 3)
          {
            bot.sendMessage(usuarios[user].id, "Seleccione un dispositivo.", "");
            continue;
          }

          switch (optionReleSelect)
          {
          case 1: // AGREGAR NUEVO RELE
            bot.sendMessage(usuarios[user].id, "✏️ Nombre para el dispositivo:", "");
            posiRel = 3;
            continue;
            break;

          case 2: // HABILITAR O DESABILIAR RELE
            if (!aparatos[releSelect].vacio)
            {
              aparatos[releSelect].activo = !aparatos[releSelect].activo;
              if (aparatos[releSelect].activo)
              {
                text = "Dispositivo " + String(aparatos[releSelect].nombre) + " HABILITADO 👍🏻";
              }
              else
              {
                text = "Dispositivo " + String(aparatos[releSelect].nombre) + " DESHABILIDADO 👎🏻";
              }

              for (int i = 0; i < cantUsuarios; i++)
              {
                if (!usuarios[i].vacio)
                {
                  bot.sendMessageWithReplyKeyboard(usuarios[user].id, text, "", menuEst(i), true);
                }
              }
              guardarReles();
              cancelar();
            }
            else
            {
              bot.sendMessage(usuarios[user].id, "Rele vacio ❌", "");
            }
            continue;
            break;

          case 3: // ELIMINAR RELE
            if (!aparatos[releSelect].vacio)
            {
              aparatos[releSelect].activo = !aparatos[releSelect].activo;
              String txt = "R" + String(releSelect);
              txt.toCharArray(aparatos[releSelect].nombre, 15);
              aparatos[releSelect].vacio = true;
              aparatos[releSelect].activo = false;
              aparatos[releSelect].estado = false;
              digitalWrite(aparatos[releSelect].pin, LOW);

              text = "Dispositivo " + String(aparatos[releSelect].nombre) + " eliminado 🗑️";
              for (int i = 0; i < cantUsuarios; i++)
              {
                if (!usuarios[i].vacio)
                {
                  bot.sendMessageWithReplyKeyboard(usuarios[i].id, text, "", menuEst(i), true);
                }
              }
              guardarReles();
              cancelar();
            }
            else
            {
              bot.sendMessage(usuarios[user].id, "Rele vacio ❌", "");
            }
            continue;
            break;

          default:
            bot.sendMessage(usuarios[user].id, "Comando incorrecto ❌", "");
            continue;
            break;
          }
          continue;
          break;
        case 3: // obtener datos para la opcion de agregar
          chat_text.toCharArray(aparatos[releSelect].nombre, 15);
          aparatos[releSelect].vacio = false;
          aparatos[releSelect].activo = true;
          aparatos[releSelect].estado = false;

          bot.sendMessageWithReplyKeyboard(usuarios[user].id, "Dispositivo " + chat_text + " agregado y habilitado 👍", "", menuEst(0), true);
          delay(15);
          guardarReles();
          cancelar();
          continue;
          break;

        default:
          continue;
          break;
        }
      }

      if (chat_text == "/reiniciar")
      {
        ultimoMsjBot = bot.messages[user].message_id;
        if (!usuarios[user].activo)
        {
          bot.sendMessage(usuarios[user].id, "El usuario esta BLOQUEADO ❌👷‍♂️", "");
          continue;
        }

        bot.sendMessage(usuarios[user].id, "Reiniciando equipo.. ⬇️", "");
        ESP.restart();
      }
      ultimoMsjBot = bot.messages[user].message_id;
      bot.sendMessage(usuarios[user].id, "Comando desconocido ❌", "");
    }
    else
    {
      ultimoMsjBot = bot.messages[user].message_id;
      bot.sendMessage(chat_id, "Usuario no permitido ❌", "");
      String mensaje = "Mi ID es: " + chat_id;
      String kJson = "[[{\"text\":\"Compartir\",\"url\":\"https://t.me/share/url?url=" + chat_name + "-" + chat_id + "\"}]]";
      bot.sendMessageWithInlineKeyboard(chat_id, mensaje, "", kJson);
      continue;
    }
  }
}

int seguridad(String id)
{
  int n = 0;
  do
  {
    if ((String(usuarios[n].id).equals(id)))
    {
      break;
    }
    n++;
  } while (n < cantUsuarios);
  return n;
}

String menuEst(byte us)
{
  String t1 = conf[0].encend ? "bloquear" : "desbloq";
  String txt;
  if (us == 0)
  {
    txt = "[[\"/control\", \"/estado\", \"/" + t1 + "\"], [\"/usuarios\", \"/controlAut\", \"/reles\"], [\"/confUsuarios\", \"/confReles\", \"/ayudaBot\"]]";
    return txt;
  }
  else
  {
    txt = "[[\"/control\", \"/estado\", \"/" + t1 + "\"], [\"/usuarios\", \"/controlAut\", \"/reles\"], [\"/ayudaBot\"]]";
    return txt;
  }
}
String estado(int usr)
{
  bool dis = false;
  String estado = "ESTADO ACTUAL - 🕑" + hActual() + "\n";
  for (int i = 0; i < cantAparatos; i++)
  {
    if (!aparatos[i].vacio)
    {
      estado += "\n";
      estado += aparatos[i].activo ? "- ✅" : "- ⛔️";
      estado += digitalRead(aparatos[i].pin) ? symbol_run : symbol_stop;
      estado += " ";
      estado += aparatos[i].nombre;
      if (aparatos[i].controlAut)
      {
        estado += " → ";
        estado += aparatos[i].contAutEst ? "🟢" : "🔴";
        estado += aparatos[i].hEnc;
        estado += " - ";
        estado += aparatos[i].hApag;
      }
      dis = true;
    }
  }

  if (!dis)
  {
    estado += "\n- No hay dispositivos.";
  }

  if (usr == 0)
  {
    bool cab = false;
    for (int j = 0; j < cantUsuarios; j++)
    {
      if (!usuarios[j].vacio && !usuarios[j].activo)
      {
        if (!cab)
        {
          estado += "\n\nUSUARIOS BLOQUEADOS 👷‍♂️";
          cab = true;
        }
        estado += "\n";
        estado += "- " + String(usuarios[j].nombre);
      }
    }
  }

  estado += "\n";
  estado += !conf[0].encend ? "\nEquipo BLOQUEADO ⛔🤖" : " ";
  estado += !usuarios[usr].activo ? "\nUsuario BLOQUEADO ⛔👷‍♂️" : " ";
  return estado;
}
String estadoUsuarios(int u)
{
  String estado = "👷🏼‍♂️ LISTA DE USUARIOS\n\n";
  for (int i = 0; i < cantUsuarios; i++)
  {
    if (!usuarios[i].vacio)
    {
      estado += usuarios[i].activo ? "✅ " : "⛔️ ";
      estado += String(i + 1);
      estado += ": ";
      estado += usuarios[i].nombre;
      if (u == 0)
      {
        estado += " - ";
        estado += usuarios[i].id;
      }
      estado += "\n";
    }
    else
    {
      estado += "❌ ";
      estado += String(i + 1);
      estado += ": Vacio";
      estado += "\n";
    }
  }
  return estado;
}
String estadoReles()
{
  String estado = "🎛 LISTA DE RELES\n\n";
  for (int i = 0; i < cantAparatos; i++)
  {
    if (!aparatos[i].vacio)
    {
      estado += aparatos[i].activo ? "✅" : "⛔️";
      estado += " Rele #";
      estado += String(i + 1);
      estado += ": ";
      estado += aparatos[i].nombre;
    }
    else
    {
      estado += "❌";
      estado += " Rele #";
      estado += String(i + 1);
      estado += ": Vacio";
    }
    estado += " → ";
    estado += aparatos[i].hEnc;
    estado += " - ";
    estado += aparatos[i].hApag;
    estado += "\n";
  }
  return estado;
}
void control(String i)
{
  String keyboardJson = "";
  for (int i = 0; i < cantAparatos; i++)
  {
    if (i == 0)
      keyboardJson += "[";

    if (!aparatos[i].vacio)
    {
      keyboardJson += "[{ \"text\" : \"";
      keyboardJson += aparatos[i].nombre;
      keyboardJson += "\", \"callback_data\" : \"";
      keyboardJson += "#" + String(aparatos[i].nombre);
      keyboardJson += "\" }]";

      if (i != cantAparatos - 1)
      {
        keyboardJson += ",";
      }
    }
  }

  int pos = keyboardJson.length() - 1;
  if (keyboardJson.charAt(pos) == ',')
  {
    keyboardJson.remove(pos);
  }
  keyboardJson += "]";
  bot.sendMessageWithInlineKeyboard(usuarios[user].id, "ON/OFF DE DISPOSITIVOS", "", keyboardJson);
}
void cambiarEstado(int mens)
{ // ON/OFF de los dispositivos conectados
  for (int i = 0; i < cantAparatos; i++)
  {
    String text = bot.messages[mens].text.substring(1);

    if (text == aparatos[i].nombre)
    {
      if (aparatos[i].activo)
      {
        aparatos[i].estado = !digitalRead(pines[i]);
        digitalWrite(pines[i], aparatos[i].estado);
        guardarReles();

        String est;
        if (digitalRead(aparatos[i].pin))
        {
          est = "Dispositivo " + String(aparatos[i].nombre) + " ENCENDIDO  " + symbol_run;
        }
        else
        {
          est = "Dispositivo " + String(aparatos[i].nombre) + " APAGADO  " + symbol_stop;
        }

        for (int i = 0; i < cantUsuarios; i++)
        {
          if (!usuarios[i].vacio)
          {
            bot.sendMessage(usuarios[i].id, est, "");
          }
        }
      }
      else
      {
        bot.sendMessage(bot.messages[mens].chat_id, "El dispositivo esta bloqueado ⚠️", "");
      }
    }
  }
}
void newMensajes(bool x)
{
  if (millis() > tAntConsulta + 1200)
  {
    int newMsj = bot.getUpdates(bot.last_message_received + 1);
    while (newMsj)
    {
      x ? telegramBot(newMsj) : (void)0;
      newMsj = bot.getUpdates(bot.last_message_received + 1);
    }
    tAntConsulta = millis();
  }
}
void confWifiManager()
{
  alertasLed(conf[0].modLed);
  WiFi.mode(WIFI_AP);
  WiFiManager wm;

  WiFiManagerParameter nuevoToken("Token", "Token de Telegram", conf[0].token, sizeof(conf[0].token));
  WiFiManagerParameter admNomb("Nombre_Admin", "Nombre del Admin", usuarios[0].nombre, sizeof(usuarios[0].nombre));
  WiFiManagerParameter admId("ID_Admin", "ID del Admin", usuarios[0].id, sizeof(usuarios[0].id));
  wm.addParameter(&nuevoToken);
  wm.addParameter(&admNomb);
  wm.addParameter(&admId);

  wm.setBreakAfterConfig(true); // Guardar a un que las credenciales sean incorrecta.
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(10);

  wm.startConfigPortal(hostname, hostpass);
  strncpy(conf[0].token, nuevoToken.getValue(), sizeof(conf[0].token));
  strncpy(usuarios[0].nombre, admNomb.getValue(), sizeof(usuarios[0].nombre));
  strncpy(usuarios[0].id, admId.getValue(), sizeof(usuarios[0].id));

  if (strlen(token) < 10)
  {
    strncpy(token, "vacio", sizeof(token));
  }

  if (strlen(usuarios[0].id) < 10)
  {
    strncpy(usuarios[0].nombre, "vacio", sizeof(usuarios[0].nombre));
    strncpy(usuarios[0].id, "0", sizeof(usuarios[0].id));
    usuarios[0].vacio = true;
    usuarios[0].activo = false;
  }
  else
  {
    usuarios[0].vacio = false;
    usuarios[0].activo = true;
  }

  if (!guardarConfig() || !guardarUsers())
  {
    Serial.println(F("Error al escribir en memoria."));
    strncpy(conf[0].msjError, "Error al escribir en memoria.", sizeof(conf[0].msjError));
    guardarConfig();
  }

  // debug();
  delay(1000);
  Serial.println(F("Reiniciando...."));
  ESP.restart();
}
void confConexiones()
{
  if (conf[0].modConfig || digitalRead(btn_AP)) // MODO CONFIGURACION
  {
    Serial.println(F("MODO CONFIGURACION"));
    conf[0].modConfig = false;
    guardarConfig();
    confWifiManager();
    delay(1000);
    Serial.println(F("Reiniciando...."));
    ESP.restart();
  }

  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(1);

  if (wm.autoConnect())
  {
    bot.updateToken(conf[0].token);
    client.setTrustAnchors(&cert);

    byte tiemp = 0;
    WiFiClient conex;
    while (!conex.connect("www.google.com", 80) && tiemp < 10)
    {
      Serial.print(conex.connect("www.google.com", 80));
      if (tiemp == 9)
      {
        Serial.println(F("NO HAY CONEXION A INTERNET"));
        strncpy(conf[0].msjError, "Sin conexion a internet.", sizeof(conf[0].msjError));
        conf[0].modConfig = true;
        conf[0].modLed = 3;
        guardarConfig();
        delay(1000);
        Serial.println(F("Reiniciando...."));
        ESP.restart();
      }
      tiemp++;
      delay(500);
    }
    conex.stop();
  }
  else
  {
    Serial.println(F("NO HAY CONEXION WIFI"));
    strncpy(conf[0].msjError, "Sin conexion WiFi.", sizeof(conf[0].msjError));
    conf[0].modConfig = true;
    conf[0].modLed = 2;
    guardarConfig();
    delay(1000);
    Serial.println(F("Reiniciando...."));
    ESP.restart();
  }

  if (strlen(usuarios[0].id) < 9 || usuarios[0].vacio || strlen(conf[0].token) < 30)
  {
    Serial.println(F("NO HAY CREDENCIALES"));
    strncpy(conf[0].msjError, "Sin credenciales.", sizeof(conf[0].msjError));
    conf[0].modConfig = true;
    conf[0].modLed = 4;
    guardarConfig();
    delay(1000);
    Serial.println(F("Reiniciando...."));
    ESP.restart();
  }
  // debug();
  Serial.println("Listo");
}
void iniciar(int i)
{
  String txt = "🚨 == ESP8266-BOT EN LINEA == 🚨";
  bot.sendMessage(usuarios[i].id, txt, "");
  txt = estado(i);

  txt += strlen(conf[0].msjError) > 1 ? ("\n⚠️ LOG DE ERROR\n- " + String(conf[0].msjError)) : " ";

  if (!usuarios[i].vacio)
  {
    if (i != 0)
    {
      bot.sendMessageWithReplyKeyboard(usuarios[i].id, txt, "", menuEst(i), true);
    }
    else
    {
      bot.sendMessageWithReplyKeyboard(usuarios[i].id, txt, "", menuEst(0), true);
    }
  }
}
void cancelar()
{
  modConfigUser = false;
  optionUserSelect = 0;
  usrSelect = 0;

  modConfigRel = false;
  optionReleSelect = 0;
  releSelect = 0;

  modConfigAut = false;
  posiAut = 0;
  disp = 0;
}
void alertasLed(byte color)
{
  switch (color)
  {
  case 0: // Ninguno - apagado
    Serial.println("LED COLOR NEGRO");
    digitalWrite(ledR, 0);
    digitalWrite(ledG, 0);
    digitalWrite(ledB, 0);
    break;

  case 1: // Sin problemas - verde
    // Serial.println("LED COLOR VERDE");
    digitalWrite(ledR, 0);
    digitalWrite(ledG, 255);
    digitalWrite(ledB, 0);
    break;

  case 2: // Sin Wifi - rojo
    Serial.println("LED COLOR ROJO");
    digitalWrite(ledR, 255);
    digitalWrite(ledG, 0);
    digitalWrite(ledB, 0);
    break;

  case 3: // Sin Conexion - azul
    Serial.println("LED COLOR AZUL");
    digitalWrite(ledR, 0);
    digitalWrite(ledG, 0);
    digitalWrite(ledB, 255);
    break;

  case 4: // Sin credenciales - amarillo
    Serial.println("LED COLOR AMARILLO");
    digitalWrite(ledR, 255);
    digitalWrite(ledG, 255);
    digitalWrite(ledB, 0);
    break;

  case 5: // Modo config. - blanco
    Serial.println("LED COLOR BLANCO");
    digitalWrite(ledR, 255);
    digitalWrite(ledG, 255);
    digitalWrite(ledB, 222);
    break;
  }
}
String hActual()
{
  char buffer[20];

  if (!getLocalTime(&timeinfo))
  {
    return "1970";
  }

  strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
  return String(buffer);
}
void controlAuto()
{
  for (int i = 0; i < cantAparatos; i++)
  {
    if (aparatos[i].contAutEst)
    {
      String x = hActual();
      if (x == String(aparatos[i].hEnc) && !aparatos[i].estado) // ENCENDIDO
      {
        if (!avisosE[i])
        {
          digitalWrite(aparatos[i].pin, HIGH);
          aparatos[i].estado = true;
          x = "Dispositivo " + String(aparatos[i].nombre) + " ENCENDIDO ▶️💡";
          for (int i = 0; i < cantUsuarios; i++)
          {
            if (!usuarios[i].vacio)
            {
              bot.sendMessage(usuarios[i].id, x, "");
              delay(1);
            }
          }
          avisosE[i] = true;
          guardarReles();
        }
      }
      else
      {
        avisosE[i] = false;
      }

      if (x == String(aparatos[i].hApag) && aparatos[i].estado) // APAGADO
      {
        if (!avisosA[i])
        {
          digitalWrite(aparatos[i].pin, LOW);
          aparatos[i].estado = false;
          x = "Dispositivo " + String(aparatos[i].nombre) + " APAGADO ⏸️💡";
          for (int i = 0; i < cantUsuarios; i++)
          {
            if (!usuarios[i].vacio)
            {
              bot.sendMessage(usuarios[i].id, x, "");
              delay(1);
            }
          }
          avisosA[i] = true;
          guardarReles();
        }
      }
      else
      {
        avisosA[i] = false;
      }
    }
  }
}

// -----------------------------------
void resetDatos()
{
  // Asegúrate de que el sistema de archivos está montado
  if (!LittleFS.begin())
  {
    Serial.println("Error al iniciar LittleFS");
    return;
  }

  String x = "vacio";

  // Configuración inicial de credenciales
  conf[0].encend = true;
  conf[0].modConfig = false;
  conf[0].modLed = 0;
  x.toCharArray(conf[0].token, sizeof(conf[0].token));
  x.toCharArray(conf[0].msjError, sizeof(conf[0].msjError));
  if (!guardarConfig())
  {
    Serial.println("Error al guardar credenciales");
  }
  delay(100);

  // Configuración inicial de usuarios
  for (int i = 0; i < cantUsuarios; i++)
  {
    x = "vacio";
    x.toCharArray(usuarios[i].nombre, sizeof(usuarios[i].nombre));
    x = "0";
    x.toCharArray(usuarios[i].id, sizeof(usuarios[i].id));
    usuarios[i].vacio = true;
    usuarios[i].activo = false;
  }
  if (!guardarUsers())
  {
    Serial.println("Error al guardar usuarios");
  }
  delay(100);

  // Configuración inicial de aparatos
  for (int i = 0; i < cantAparatos; i++)
  {
    aparatos[i].pin = pines[i];
    x = "R" + String(i + 1);
    x.toCharArray(aparatos[i].nombre, sizeof(aparatos[i].nombre));
    aparatos[i].vacio = true;
    aparatos[i].activo = false;
    aparatos[i].estado = false;
    aparatos[i].controlAut = false;
    aparatos[i].contAutEst = false;
    x = "N/A";
    x.toCharArray(aparatos[i].hEnc, sizeof(aparatos[i].hEnc));
    x.toCharArray(aparatos[i].hApag, sizeof(aparatos[i].hApag));
  }
  if (!guardarReles())
  {
    Serial.println("Error al guardar aparatos");
  }
  delay(1000);

  // Opcional: Reiniciar el ESP
  ESP.restart();
}
bool cargarDatos()
{
  if (!cargarConfig())
  {
    Serial.println("Error al cargar las configuracioes");
    delay(100);
  }
  if (!cargarUsers())
  {
    Serial.println("Error al cargar los usuarios");
    delay(100);
  }
  if (!cargarReles())
  {
    Serial.println("Error al cargar los dispositivos");
    delay(100);
    return false;
  }
  return true;
}
bool cargarConfig()
{
  if (!LittleFS.begin())
  {
    Serial.println(F("Fallido para montar sistema de archivos creden"));
    return false;
  }

  File file = LittleFS.open("/creden.json", "r");
  if (!file)
  {
    Serial.println("Error al abrir archivo para cargar credenciales.");
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    Serial.print("Error al leer credenciales: ");
    Serial.println(error.c_str());
    file.close();
    return false;
  }

  conf[0].encend = doc["encend"];
  conf[0].modConfig = doc["modConfig"];
  conf[0].modLed = doc["modLed"];
  strlcpy(conf[0].token, doc["token"], sizeof(conf[0].token));
  strlcpy(conf[0].msjError, doc["msjError"], sizeof(conf[0].msjError));

  file.close();
  return true;
}
bool cargarUsers()
{
  if (!LittleFS.begin())
  {
    Serial.println(F("Fallido para montar sistema de archivos users"));
    return false;
  }

  File file = LittleFS.open("/usuarios.json", "r");
  if (!file)
  {
    Serial.println("Error al abrir archivo para cargar usuarios.");
    return false;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    Serial.print("Error al leer usuarios: ");
    Serial.println(error.c_str());
    file.close();
    return false;
  }

  JsonArray usuariosArray = doc.as<JsonArray>();
  int i = 0;
  for (JsonObject user : usuariosArray)
  {
    strlcpy(usuarios[i].id, user["id"], sizeof(usuarios[i].id));
    strlcpy(usuarios[i].nombre, user["nombre"], sizeof(usuarios[i].nombre));
    usuarios[i].vacio = user["vacio"];
    usuarios[i].activo = user["activo"];
    i++;
  }

  file.close();
  return true;
}
bool cargarReles()
{
  if (!LittleFS.begin())
  {
    Serial.println(F("Fallido para montar sistema de archivos reles"));
    return false;
  }

  File file = LittleFS.open("/aparatos.json", "r");
  if (!file)
  {
    Serial.println("Error al abrir archivo para cargar aparatos.");
    return false;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    Serial.print("Error al leer aparatos: ");
    Serial.println(error.c_str());
    file.close();
    return false;
  }

  JsonArray aparatosArray = doc.as<JsonArray>();
  int i = 0;
  for (JsonObject aparato : aparatosArray)
  {
    aparatos[i].pin = aparato["pin"];
    strlcpy(aparatos[i].nombre, aparato["nombre"], sizeof(aparatos[i].nombre));
    aparatos[i].vacio = aparato["vacio"];
    aparatos[i].activo = aparato["activo"];
    aparatos[i].estado = aparato["estado"];
    aparatos[i].controlAut = aparato["controlAut"];
    aparatos[i].contAutEst = aparato["contAutEst"];
    strlcpy(aparatos[i].hEnc, aparato["hEnc"], sizeof(aparatos[i].hEnc));
    strlcpy(aparatos[i].hApag, aparato["hApag"], sizeof(aparatos[i].hApag));
    i++;
  }

  file.close();
  return true;
}
bool guardarConfig()
{
  File file = LittleFS.open("/creden.json", "w");
  if (!file)
  {
    Serial.println("Error al abrir archivo para guardar credenciales. G");
    return false;
  }

  StaticJsonDocument<256> doc;
  doc["encend"] = conf[0].encend;
  doc["modConfig"] = conf[0].modConfig;
  doc["modLed"] = conf[0].modLed;
  doc["token"] = conf[0].token;
  doc["msjError"] = conf[0].msjError;

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("Error al escribir credenciales en archivo. G");
    return false;
  }
  // serializeJson(doc, Serial);
  Serial.println();
  file.close();
  return true;
}
bool guardarUsers()
{
  File file = LittleFS.open("/usuarios.json", "w");
  if (!file)
  {
    Serial.println("Error al abrir archivo para guardar usuarios. G");
    return false;
  }

  StaticJsonDocument<512> doc;
  JsonArray usuariosArray = doc.to<JsonArray>();

  for (int i = 0; i < 4; i++)
  {
    JsonObject user = usuariosArray.createNestedObject();
    user["id"] = usuarios[i].id;
    user["nombre"] = usuarios[i].nombre;
    user["vacio"] = usuarios[i].vacio;
    user["activo"] = usuarios[i].activo;
  }

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("Error al escribir usuarios en archivo. G");
    return false;
  }
  file.close();
  return true;
}
bool guardarReles()
{
  File file = LittleFS.open("/aparatos.json", "w");
  if (!file)
  {
    Serial.println("Error al abrir archivo para guardar aparatos. G");
    return false;
  }

  StaticJsonDocument<512> doc;
  JsonArray aparatosArray = doc.to<JsonArray>();

  for (int i = 0; i < 4; i++)
  {
    JsonObject aparato = aparatosArray.createNestedObject();
    aparato["pin"] = aparatos[i].pin;
    aparato["nombre"] = aparatos[i].nombre;
    aparato["vacio"] = aparatos[i].vacio;
    aparato["activo"] = aparatos[i].activo;
    aparato["estado"] = aparatos[i].estado;
    aparato["controlAut"] = aparatos[i].controlAut;
    aparato["contAutEst"] = aparatos[i].contAutEst;
    aparato["hEnc"] = aparatos[i].hEnc;
    aparato["hApag"] = aparatos[i].hApag;
  }

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("Error al escribir aparatos en archivo. G");
    return false;
  }
  file.close();
  return true;
}

// DEBUG -----------------------------
void testReles()
{
  for (byte i = 0; i < 4; i++)
  {
    digitalWrite(pines[i], LOW);
  }

  for (byte i = 0; i < 12; i++)
  {
    for (byte i = 0; i < 4; i++)
    {
      digitalWrite(pines[i], !digitalRead(pines[i]));
      delay(400);
    }
    delay(2000);
  }

  for (byte i = 0; i < 4; i++)
  {
    digitalWrite(pines[i], aparatos[i].estado);
  }
}
void debug()
{
  Serial.println("\n-------LLAVES-------");
  Serial.println("ENCEN\tCONFIG\tLED\tTOKEN");
  Serial.print(conf[0].encend);
  Serial.print('\t');
  Serial.print(conf[0].modConfig);
  Serial.print('\t');
  Serial.print(conf[0].modLed);
  Serial.print('\t');
  Serial.println(conf[0].token);

  Serial.println("\n-------USUARIOS------");
  Serial.println("CODIGO ID\tNOMBRE\tVACIO\tACTIVO");
  for (int i = 0; i < 4; i++)
  {
    Serial.print(usuarios[i].id);
    Serial.print('\t');
    Serial.print(usuarios[i].nombre);
    Serial.print('\t');
    Serial.print(usuarios[i].vacio);
    Serial.print('\t');
    Serial.println(usuarios[i].activo);
  }

  Serial.println("\n-------APARATOS-------");
  Serial.println("PIN\tNOMBRE\tVACIO\tACTIVO\tESTADO\tCONTROLAUT\tESTCONTROLAUT\tHORA_ENC\tHORA_APAG");
  for (int i = 0; i < 4; i++)
  {
    Serial.print(aparatos[i].pin);
    Serial.print('\t');
    Serial.print(aparatos[i].nombre);
    Serial.print('\t');
    // Serial.print('\t');
    Serial.print(aparatos[i].vacio);
    Serial.print('\t');
    Serial.print(aparatos[i].activo);
    Serial.print('\t');
    Serial.print(aparatos[i].estado);
    Serial.print('\t');
    Serial.print(aparatos[i].controlAut);
    Serial.print('\t');
    Serial.print(aparatos[i].contAutEst);
    Serial.print('\t');
    Serial.print(aparatos[i].hEnc);
    Serial.print('\t');
    Serial.println(aparatos[i].hApag);
  }
}

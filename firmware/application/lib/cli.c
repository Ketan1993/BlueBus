/*
 * File:   cli.c
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     Implement a CLI to pass commands to the device
 */
#include "cli.h"
#include <stdlib.h>

/**
 * CLIInit()
 *     Description:
 *         Initialize our CLI object
 *     Params:
 *         UART_t *uart - A pointer to the UART module object
 *         BC127_t *bt - A pointer to the BC127 object
 *         IBus_t *bt - A pointer to the IBus object
 *     Returns:
 *         void
 */
CLI_t CLIInit(UART_t *uart, BC127_t *bt, IBus_t *ibus)
{
    CLI_t cli;
    cli.uart = uart;
    cli.bt = bt;
    cli.ibus = ibus;
    cli.lastChar = 0;
    return cli;
}

/**
 * CLIProcess()
 *     Description:
 *         Read the RX queue and process the messages into meaningful data
 *     Params:
 *         UART_t *uart - A pointer to the UART module object
 *     Returns:
 *         void
 */
void CLIProcess(CLI_t *cli)
{
    while (cli->lastChar != cli->uart->rxQueue.writeCursor) {
        UARTSendChar(cli->uart, CharQueueGet(&cli->uart->rxQueue, cli->lastChar));
        if (cli->lastChar >= 255) {
            cli->lastChar = 0;
        } else {
            cli->lastChar++;
        }
    }
    uint8_t messageLength = CharQueueSeek(&cli->uart->rxQueue, CLI_MSG_END_CHAR);
    if (messageLength > 0) {
        // Send a newline to keep the CLI pretty
        UARTSendChar(cli->uart, 0x0A);
        char msg[messageLength];
        uint8_t i;
        uint8_t delimCount = 1;
        for (i = 0; i < messageLength; i++) {
            char c = CharQueueNext(&cli->uart->rxQueue);
            if (c == CLI_MSG_DELIMETER) {
                delimCount++;
            }
            if (c != CLI_MSG_END_CHAR) {
                msg[i] = c;
            } else {
                // 0x0D delimits messages, so we change it to a null
                // terminator instead
                msg[i] = '\0';
            }
        }
        // Copy the message, since strtok adds a null terminator after the first
        // occurrence of the delimiter, it will not cause issues with string
        // functions
        char tmpMsg[messageLength];
        strcpy(tmpMsg, msg);
        char *msgBuf[delimCount];
        char *p = strtok(tmpMsg, " ");
        i = 0;
        while (p != NULL) {
            msgBuf[i++] = p;
            p = strtok(NULL, " ");
        }
        uint8_t cmdSuccess = 1;
        if (strcmp(msgBuf[0], "BOOTLOADER") == 0) {
            LogRaw("Rebooting into bootloader\r\n");
            ConfigSetBootloaderMode(0x01);
            __asm__ volatile ("reset");
        } else if (strcmp(msgBuf[0], "BTREBOOT") == 0) {
            BC127CommandReset(cli->bt);
        } else if (strcmp(msgBuf[0], "BTRESETPDL") == 0) {
            BC127CommandUnpair(cli->bt);
        } else if (strcmp(msgBuf[0], "BTWRITE") == 0) {
            BC127CommandWrite(cli->bt);
        } else if (strcmp(msgBuf[0], "GET") == 0) {
            if (strcmp(msgBuf[1], "BTCFG") == 0) {
                BC127SendCommand(cli->bt, "CONFIG");
            } else if (strcmp(msgBuf[1], "IBUS") == 0) {
                IBusCommandDIAGetIdentity(cli->ibus, IBUS_DEVICE_GT);
                IBusCommandDIAGetIdentity(cli->ibus, IBUS_DEVICE_RAD);
                IBusCommandDIAGetIdentity(cli->ibus, IBUS_DEVICE_LCM);
            } else if (strcmp(msgBuf[1], "IBUSC") == 0) {
                IBusCommandDIAGetCodingData(cli->ibus, IBUS_DEVICE_GT, 0x00);
            } else if (strcmp(msgBuf[1], "HFP") == 0) {
                if (ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_ON) {
                    LogRaw("HFP: On\r\n");
                } else {
                    LogRaw("HFP: Off\r\n");
                }
            } else if (strcmp(msgBuf[1], "UI") == 0) {
                unsigned char uiMode = ConfigGetUIMode();
                if (uiMode == IBus_UI_CD53) {
                    LogRaw("UI Mode: CD53\r\n");
                } else if (uiMode == IBus_UI_BMBT) {
                    LogRaw("UI Mode: BMBT\r\n");
                } else if (uiMode == IBus_UI_MID) {
                    LogRaw("UI Mode: MID\r\n");
                } else if (uiMode == IBus_UI_MID_BMBT) {
                    LogRaw("UI Mode: MID / BMBT\r\n");
                } else {
                    LogRaw("UI Mode: Not set or Invalid\r\n");
                }
            } else {
                cmdSuccess = 0;
            }
        } else if (strcmp(msgBuf[0], "REBOOT") == 0) {
            __asm__ volatile ("reset");
        } else if (strcmp(msgBuf[0], "SET") == 0) {
            if (strcmp(msgBuf[1], "AUDIO") == 0) {
                if (strcmp(msgBuf[2], "ANALOG") == 0) {
                    BC127CommandSetAudioDigital(
                        cli->bt,
                        BC127_AUDIO_I2S,
                        "44100",
                        "64",
                        "100800"
                    );
                    BC127CommandReset(cli->bt);
                } else if (strcmp(msgBuf[2], "DIGITAL") == 0) {
                    BC127CommandSetAudioDigital(
                        cli->bt,
                        BC127_AUDIO_SPDIF,
                        "44100",
                        "0",
                        "000000"
                    );
                    BC127CommandReset(cli->bt);
                } else {
                    cmdSuccess = 0;
                }
            } else if (strcmp(msgBuf[1], "BCINIT") == 0) {
                BC127CommandSetAudio(cli->bt, 0, 1);
                BC127CommandSetAudioAnalog(cli->bt, "11", "15", "1", "OFF");
                BC127CommandSetAudioDigital(
                    cli->bt,
                    BC127_AUDIO_I2S,
                    "44100",
                    "64",
                    "100800"
                );
                BC127CommandSetBtState(cli->bt, 2, 2);
                BC127CommandSetCodec(cli->bt, 1, "OFF");
                BC127CommandSetMetadata(cli->bt, 1);
                BC127CommandSetModuleName(cli->bt, "BlueBus");
                BC127CommandSetProfiles(cli->bt, 1, 1, 0, 1);
                BC127CommandSetUART(cli->bt, 9600, "OFF", 0);
            } else if (strcmp(msgBuf[1], "HFP") == 0) {
                if (strcmp(msgBuf[2], "ON") == 0) {
                    ConfigSetSetting(CONFIG_SETTING_HFP, CONFIG_SETTING_ON);
                    BC127CommandSetProfiles(cli->bt, 1, 1, 0, 1);
                } else if (strcmp(msgBuf[2], "OFF") == 0) {
                    ConfigSetSetting(CONFIG_SETTING_HFP, CONFIG_SETTING_OFF);
                    BC127CommandSetProfiles(cli->bt, 1, 1, 0, 0);
                } else {
                    cmdSuccess = 0;
                }
                LogRaw("HFP Toggled: Reset BT to complete\r\n");
            } else if (strcmp(msgBuf[1], "UI") == 0) {
                if (strcmp(msgBuf[2], "1") == 0) {
                    ConfigSetUIMode(IBus_UI_CD53);
                    LogRaw("UI Mode: CD53\r\n");
                } else if (strcmp(msgBuf[2], "2") == 0) {
                    ConfigSetUIMode(IBus_UI_BMBT);
                    LogRaw("UI Mode: BMBT\r\n");
                } else if (strcmp(msgBuf[2], "3") == 0) {
                    ConfigSetUIMode(IBus_UI_MID);
                    LogRaw("UI Mode: MID\r\n");
                } else if (strcmp(msgBuf[2], "4") == 0) {
                    ConfigSetUIMode(IBus_UI_MID_BMBT);
                    LogRaw("UI Mode: MID / BMBT\r\n");
                } else {
                    LogRaw("Invalid UI Mode specified\r\n");
                }
            } else if(strcmp(msgBuf[1], "IGN") == 0) {
                if (strcmp(msgBuf[2], "0") == 0) {
                    IBusCommandIgnitionStatus(cli->ibus, 0x00);
                    cli->ibus->ignitionStatus = 0;
                    EventTriggerCallback(IBusEvent_IgnitionStatus, 0x00);
                } else if (strcmp(msgBuf[2], "1") == 0) {
                    IBusCommandIgnitionStatus(cli->ibus, 0x01);
                    cli->ibus->ignitionStatus = 1;
                    EventTriggerCallback(IBusEvent_IgnitionStatus, 0x00);
                } else {
                    cmdSuccess = 0;
                }
            } else if (strcmp(msgBuf[1], "LIGHT") == 0) {
                if (strcmp(msgBuf[2], "OFF") == 0) {
                    IBusCommandDIATerminateDiag(cli->ibus, IBUS_DEVICE_LCM);
                } else if (strcmp(msgBuf[2], "TR") == 0) {
                    IBusCommandLCMEnableBlinker(cli->ibus, 0x40);
                } else if (strcmp(msgBuf[2], "TL") == 0) {
                    IBusCommandLCMEnableBlinker(cli->ibus, 0x80);
                } else {
                    cmdSuccess = 0;
                }
            } else if (strcmp(msgBuf[1], "LOG") == 0) {
                unsigned char system = 0xFF;
                unsigned char value = 0xFF;
                // Get the system
                if (strcmp(msgBuf[2], "BT") == 0) {
                    system = CONFIG_DEVICE_LOG_BT;
                } else if (strcmp(msgBuf[2], "IBUS") == 0) {
                    system = CONFIG_DEVICE_LOG_IBUS;
                } else if (strcmp(msgBuf[2], "SYS") == 0) {
                    system = CONFIG_DEVICE_LOG_SYSTEM;
                } else if (strcmp(msgBuf[2], "UI") == 0) {
                    system = CONFIG_DEVICE_LOG_UI;
                }
                // Get the value
                if (strcmp(msgBuf[3], "OFF") == 0) {
                    value = 0;
                } else if (strcmp(msgBuf[3], "ON") == 0) {
                    value = 1;
                }
                if (system != 0xFF && value != 0xFF) {
                    ConfigSetLog(system, value);
                } else {
                    LogRaw("Invalid Parameters for SET LOG\r\n");
                }
            } else if (strcmp(msgBuf[1], "CVC") == 0) {
                if (strcmp(msgBuf[2], "ON") == 0) {
                    BC127SendCommand(cli->bt, "SET HFP_CONFIG=ON ON OFF ON OFF OFF");
                } else if (strcmp(msgBuf[2], "OFF") == 0) {
                    BC127SendCommand(cli->bt, "SET HFP_CONFIG=OFF ON OFF OFF OFF OFF");
                }
                BC127CommandWrite(cli->bt);
                BC127CommandReset(cli->bt);
            } else {
                cmdSuccess = 0;
            }
        } else if (strcmp(msgBuf[0], "HELP") == 0 || strlen(msgBuf[0]) == 0) {
            LogRaw("BlueBus Firmware version: 1.0.7\r\n");
            LogRaw("Available Commands:\r\n");
            LogRaw("    BOOTLOADER - Reboot into the bootloader immediately\r\n");
            LogRaw("    BTREBOOT - Reboot the BC127\r\n");
            LogRaw("    BTRESETPDL - Unpair all devices from the BC127\r\n");
            LogRaw("    GET BTCFG - Get the BC127 Configuration\r\n");
            LogRaw("    GET HFP - Get the current HFP mode\r\n");
            LogRaw("    GET IBUS - Get debug info from the IBus\r\n");
            LogRaw("    GET UI - Get the current UI Mode\r\n");
            LogRaw("    REBOOT - Reboot the device\r\n");
            LogRaw("    SET AUDIO x - Set the audio output where x is ANALOG");
            LogRaw(" or DIGITAL. DIGITAL is the coax output.\r\n");
            LogRaw("    SET HFP x - Enable or Disable HFP x = ON or OFF\r\n");
            LogRaw("    SET IGN x - Send the ignition status message [DEBUG]\r\n");
            LogRaw("    SET LOG x y - Change logging for x (BT, IBUS, SYS, UI)");
            LogRaw(" to ON or OFF\r\n");
            LogRaw("    SET UI x - Set the UI to x, ");
            LogRaw("where 1 is CD53 (Business Radio), 2 is BMBT (Nav) ");
            LogRaw("3 is MID (Multi-Info Display) and 4 is BMBT / MID\r\n");
        } else {
            cmdSuccess = 0;
        }
        if (cmdSuccess == 0) {
            LogRaw("Command Unknown or invalid. Try HELP\r\n");
        } else {
            LogRaw("OK\r\n");
        }
    }
}

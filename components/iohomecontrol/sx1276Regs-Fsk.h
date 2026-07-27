#ifndef __SX1276_REGS_FSK_H__
#define __SX1276_REGS_FSK_H__

#define REG_FIFO                                    0x00
#define REG_OPMODE                                  0x01
#define REG_BITRATEMSB                              0x02
#define REG_BITRATELSB                              0x03
#define REG_FDEVMSB                                 0x04
#define REG_FDEVLSB                                 0x05
#define REG_FRFMSB                                  0x06
#define REG_FRFMID                                  0x07
#define REG_FRFLSB                                  0x08
#define REG_PACONFIG                                0x09
#define REG_PARAMP                                  0x0A
#define REG_OCP                                     0x0B
#define REG_LNA                                     0x0C
#define REG_RXCONFIG                                0x0D
#define REG_RSSICONFIG                              0x0E
#define REG_RSSICOLLISION                           0x0F
#define REG_RSSITHRESH                              0x10
#define REG_RSSIVALUE                               0x11
#define REG_RXBW                                    0x12
#define REG_AFCBW                                   0x13
#define REG_OOKPEAK                                 0x14
#define REG_OOKFIX                                  0x15
#define REG_OOKAVG                                  0x16
#define REG_RES17                                   0x17
#define REG_RES18                                   0x18
#define REG_RES19                                   0x19
#define REG_AFCFEI                                  0x1A
#define REG_AFCMSB                                  0x1B
#define REG_AFCLSB                                  0x1C
#define REG_FEIMSB                                  0x1D
#define REG_FEILSB                                  0x1E
#define REG_PREAMBLEDETECT                          0x1F
#define REG_RXTIMEOUT1                              0x20
#define REG_RXTIMEOUT2                              0x21
#define REG_RXTIMEOUT3                              0x22
#define REG_RXDELAY                                 0x23
#define REG_OSC                                     0x24
#define REG_PREAMBLEMSB                             0x25
#define REG_PREAMBLELSB                             0x26
#define REG_SYNCCONFIG                              0x27
#define REG_SYNCVALUE1                              0x28
#define REG_SYNCVALUE2                              0x29
#define REG_SYNCVALUE3                              0x2A
#define REG_SYNCVALUE4                              0x2B
#define REG_SYNCVALUE5                              0x2C
#define REG_SYNCVALUE6                              0x2D
#define REG_SYNCVALUE7                              0x2E
#define REG_SYNCVALUE8                              0x2F
#define REG_PACKETCONFIG1                           0x30
#define REG_PACKETCONFIG2                           0x31
#define REG_PAYLOADLENGTH                           0x32
#define REG_NODEADRS                                0x33
#define REG_BROADCASTADRS                           0x34
#define REG_FIFOTHRESH                              0x35
#define REG_SEQCONFIG1                              0x36
#define REG_SEQCONFIG2                              0x37
#define REG_TIMERRESOL                              0x38
#define REG_TIMER1COEF                              0x39
#define REG_TIMER2COEF                              0x3A
#define REG_IMAGECAL                                0x3B
#define REG_TEMP                                    0x3C
#define REG_LOWBAT                                  0x3D
#define REG_IRQFLAGS1                               0x3E
#define REG_IRQFLAGS2                               0x3F
#define REG_DIOMAPPING1                             0x40
#define REG_DIOMAPPING2                             0x41
#define REG_VERSION                                 0x42
#define REG_PLLHOP                                  0x44
#define REG_TCXO                                    0x4B
#define REG_PADAC                                   0x4D
#define REG_FORMERTEMP                              0x5B
#define REG_BITRATEFRAC                             0x5D
#define REG_AGCREF                                  0x61
#define REG_AGCTHRESH1                              0x62
#define REG_AGCTHRESH2                              0x63
#define REG_AGCTHRESH3                              0x64
#define REG_PLL                                     0x70

#define RF_OPMODE_LONGRANGEMODE_MASK                0x7F
#define RF_OPMODE_LONGRANGEMODE_OFF                 0x00
#define RF_OPMODE_LONGRANGEMODE_ON                  0x80

#define RF_OPMODE_MODULATIONTYPE_MASK               0x9F
#define RF_OPMODE_MODULATIONTYPE_FSK                0x00
#define RF_OPMODE_MODULATIONTYPE_OOK                0x20

#define RF_OPMODE_MODULATIONSHAPING_MASK            0xE7
#define RF_OPMODE_MODULATIONSHAPING_00              0x00
#define RF_OPMODE_MODULATIONSHAPING_01              0x08
#define RF_OPMODE_MODULATIONSHAPING_10              0x10
#define RF_OPMODE_MODULATIONSHAPING_11              0x18

#define RF_OPMODE_MASK                              0xF8
#define RF_OPMODE_SLEEP                             0x00
#define RF_OPMODE_STANDBY                           0x01
#define RF_OPMODE_SYNTHESIZER_TX                    0x02
#define RF_OPMODE_TRANSMITTER                       0x03
#define RF_OPMODE_SYNTHESIZER_RX                    0x04
#define RF_OPMODE_RECEIVER                          0x05

#define RF_PACONFIG_PASELECT_MASK                   0x7F
#define RF_PACONFIG_PASELECT_PABOOST                0x80
#define RF_PACONFIG_PASELECT_RFO                    0x00

#define RF_PACONFIG_MAX_POWER_MASK                  0x8F
#define RF_PACONFIG_OUTPUTPOWER_MASK                0xF0

#define RF_PARAMP_MODULATIONSHAPING_MASK            0x9F
#define RF_PARAMP_MODULATIONSHAPING_00              0x00
#define RF_PARAMP_MODULATIONSHAPING_01              0x20
#define RF_PARAMP_MODULATIONSHAPING_10              0x40
#define RF_PARAMP_MODULATIONSHAPING_11              0x60

#define RF_PARAMP_LOWPNTXPLL_MASK                   0xEF
#define RF_PARAMP_LOWPNTXPLL_OFF                    0x10
#define RF_PARAMP_LOWPNTXPLL_ON                     0x00

#define RF_PARAMP_MASK                              0xF0
#define RF_PARAMP_3400_US                           0x00
#define RF_PARAMP_2000_US                           0x01
#define RF_PARAMP_1000_US                           0x02
#define RF_PARAMP_0500_US                           0x03
#define RF_PARAMP_0250_US                           0x04
#define RF_PARAMP_0125_US                           0x05
#define RF_PARAMP_0100_US                           0x06
#define RF_PARAMP_0062_US                           0x07
#define RF_PARAMP_0050_US                           0x08
#define RF_PARAMP_0040_US                           0x09
#define RF_PARAMP_0031_US                           0x0A
#define RF_PARAMP_0025_US                           0x0B
#define RF_PARAMP_0020_US                           0x0C
#define RF_PARAMP_0015_US                           0x0D
#define RF_PARAMP_0012_US                           0x0E
#define RF_PARAMP_0010_US                           0x0F

#define RF_OCP_MASK                                 0xDF
#define RF_OCP_ON                                   0x20
#define RF_OCP_OFF                                  0x00

#define RF_OCP_TRIM_MASK                            0xE0
#define RF_OCP_TRIM_240_MA                          0x1B

#define RF_LNA_GAIN_MASK                            0x1F
#define RF_LNA_GAIN_G1                              0x20
#define RF_LNA_GAIN_G2                              0x40
#define RF_LNA_GAIN_G3                              0x60
#define RF_LNA_GAIN_G4                              0x80
#define RF_LNA_GAIN_G5                              0xA0
#define RF_LNA_GAIN_G6                              0xC0

#define RF_LNA_BOOST_MASK                           0xFC
#define RF_LNA_BOOST_OFF                            0x00
#define RF_LNA_BOOST_ON                             0x03

#define RF_RXCONFIG_RESTARTRXONCOLLISION_MASK       0x7F
#define RF_RXCONFIG_RESTARTRXONCOLLISION_ON         0x80
#define RF_RXCONFIG_RESTARTRXONCOLLISION_OFF        0x00

#define RF_RXCONFIG_AFCAUTO_MASK                    0xEF
#define RF_RXCONFIG_AFCAUTO_ON                      0x10
#define RF_RXCONFIG_AFCAUTO_OFF                     0x00

#define RF_RXCONFIG_AGCAUTO_MASK                    0xF7
#define RF_RXCONFIG_AGCAUTO_ON                      0x08
#define RF_RXCONFIG_AGCAUTO_OFF                     0x00

#define RF_RXCONFIG_RXTRIGER_MASK                   0xF8
#define RF_RXCONFIG_RXTRIGER_OFF                    0x00
#define RF_RXCONFIG_RXTRIGER_RSSI                   0x01
#define RF_RXCONFIG_RXTRIGER_PREAMBLEDETECT         0x06
#define RF_RXCONFIG_RXTRIGER_RSSI_PREAMBLEDETECT    0x07

#define RF_RSSICONFIG_SMOOTHING_MASK                0xF8
#define RF_RSSICONFIG_SMOOTHING_8                   0x02

#define RF_RXBW_MANT_16                             0x00
#define RF_RXBW_EXP_1                               0x01

#define RF_AFCBW_MANTAFC_16                         0x00
#define RF_AFCBW_EXPAFC_1                           0x01

#define RF_PREAMBLEDETECT_DETECTOR_MASK             0x7F
#define RF_PREAMBLEDETECT_DETECTOR_ON               0x80
#define RF_PREAMBLEDETECT_DETECTOR_OFF              0x00

#define RF_PREAMBLEDETECT_DETECTORSIZE_MASK         0x9F
#define RF_PREAMBLEDETECT_DETECTORSIZE_2            0x20

#define RF_PREAMBLEDETECT_DETECTORTOL_MASK          0xE0
#define RF_PREAMBLEDETECT_DETECTORTOL_10            0x0A

#define RF_OSC_CLKOUT_MASK                          0xF8
#define RF_OSC_CLKOUT_OFF                           0x07

#define RF_SYNCCONFIG_AUTORESTARTRXMODE_WAITPLL_OFF 0x40
#define RF_SYNCCONFIG_PREAMBLEPOLARITY_AA           0x00
#define RF_SYNCCONFIG_SYNC_ON                       0x10
#define RF_SYNCCONFIG_SYNCSIZE_MASK                 0xF8
#define RF_SYNCCONFIG_SYNCSIZE_2                    0x01
#define RF_SYNCCONFIG_SYNCSIZE_3                    0x02

#define RF_PACKETCONFIG1_PACKETFORMAT_VARIABLE      0x80
#define RF_PACKETCONFIG1_DCFREE_OFF                 0x00
#define RF_PACKETCONFIG1_CRC_ON                     0x10
#define RF_PACKETCONFIG1_CRCAUTOCLEAR_ON            0x00
#define RF_PACKETCONFIG1_CRCWHITENINGTYPE_CCITT     0x00
#define RF_PACKETCONFIG1_ADDRSFILTERING_OFF          0x00

#define RF_PACKETCONFIG2_DATAMODE_PACKET            0x40
#define RF_PACKETCONFIG2_IOHOME_ON                  0x20

#define RF_FIFOTHRESH_TXSTARTCONDITION_FIFONOTEMPTY 0x80

#define RF_IMAGECAL_AUTOIMAGECAL_MASK               0x7F
#define RF_IMAGECAL_IMAGECAL_MASK                   0xBF
#define RF_IMAGECAL_IMAGECAL_START                  0x40
#define RF_IMAGECAL_IMAGECAL_RUNNING                0x20

#define RF_IRQFLAGS1_MODEREADY                      0x80
#define RF_IRQFLAGS1_RXREADY                        0x40
#define RF_IRQFLAGS1_TXREADY                        0x20
#define RF_IRQFLAGS1_PLLLOCK                        0x10
#define RF_IRQFLAGS1_RSSI                           0x08
#define RF_IRQFLAGS1_TIMEOUT                        0x04
#define RF_IRQFLAGS1_PREAMBLEDETECT                 0x02
#define RF_IRQFLAGS1_SYNCADDRESSMATCH               0x01

#define RF_IRQFLAGS2_FIFOFULL                       0x80
#define RF_IRQFLAGS2_FIFOEMPTY                      0x40
#define RF_IRQFLAGS2_FIFOLEVEL                      0x20
#define RF_IRQFLAGS2_FIFOOVERRUN                    0x10
#define RF_IRQFLAGS2_PACKETSENT                     0x08
#define RF_IRQFLAGS2_PAYLOADREADY                   0x04
#define RF_IRQFLAGS2_CRCOK                          0x02
#define RF_IRQFLAGS2_LOWBAT                         0x01

#define RF_DIOMAPPING1_DIO0_00                      0x00
#define RF_DIOMAPPING1_DIO1_01                      0x10
#define RF_DIOMAPPING1_DIO2_11                      0x0C
#define RF_DIOMAPPING1_DIO3_01                      0x01

#define RF_DIOMAPPING2_MAP_PREAMBLEDETECT           0x01
#define RF_DIOMAPPING2_DIO4_11                      0xC0
#define RF_DIOMAPPING2_DIO5_10                      0x20

#define RF_PLLHOP_FASTHOP_ON                        0x80

#endif

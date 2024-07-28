# Sequence diagrams

This file details a graphical represntation of the functional system states and processes.

**Table of Contents**
- [Sequence diagrams](#sequence-diagrams)
  - [Boot flow](#boot-flow)
  - [Wi-Fi loss recovery](#wi-fi-loss-recovery)

## Boot flow

Occurs when the device is booted.

```mermaid
flowchart TD
    A(Boot) --> B[Show splash screen]
    B --> C[Countdown wait for \n Wi-Fi Primary SSID]
    C --> D{Wi-Fi connect \n successful?}
    D -->|Yes| E[Ping all \n targets]
    D -->|No| F{Failed SSID was \n primary?}
    F -->|Yes| G[Countdown wait for \n Wi-Fi Secondary SSID]
    F -->|No| H[Show connection \n failed menu]
    G --> D
    H --> I{User Input}
    I -->|Retry STA| C
    I -->|Start AP| J[Start AP]
    I -->|Cancel| L[Go to \n homepage]
    J --> K[Go to \n Wi-Fi page]
    E --> M{Router ping \n successful?}
    M -->|No| E
    M -->|Yes| N[Connect to \n router]
    N --> O{Router connect \n successful?}
    O -->|No| E
    O -->|Yes| P[Go to WAN \n list page]
    P --> Q(Boot sequence \n complete)
    I --> Q
    K --> Q
```

## Wi-Fi loss recovery

Occurs when a loss in Wi-Fi connection is detected and attempts to restore it.

```mermaid
flowchart TD
    A(Wi-Fi disconnect \n detected) --> B[Save the currrent screen]
    B --> C[Countdown wait for \n Wi-Fi Primary SSID]
    C --> D{Wi-Fi connect \n successful?}
    D -->|Yes| E[Return to previous screen]
    D -->|No| F{Failed SSID was \n primary?}
    F -->|Yes| G[Countdown wait for \n Wi-Fi Secondary SSID]
    F -->|No| H[Show connection \n failed menu]
    G --> D
    H --> I{User Input}
    I -->|Retry STA| C
    I -->|Start AP| J[Start AP]
    I -->|Cancel| L[Go to \n homepage]
    J --> K[Go to \n Wi-Fi page]
    E --> Q[Wi-Fi reconnect \n complete]
    K --> Q
    I --> Q
    K --> Q
```
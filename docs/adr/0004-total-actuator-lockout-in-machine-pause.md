# Total Actuator Lockout in Machine Pause

We decided that entering Machine Pause triggers a complete hardware actuation lockout on the Actuator (Board B), suppressing both the DC conveyor motor and remote servo gate movements, alongside blocking shape counter increments. This guarantees a strict safety envelope where neither local automatic logic nor remote dashboard commands can trigger physical motion while an operator has paused the machine.

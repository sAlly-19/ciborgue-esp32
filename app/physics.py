class SpringValue:
    """
    Oscilador harmônico amortecido (Física de Mola) para animações orgânicas
    com Squash & Stretch e overshoot natural estilo Cozmo / Eilik.
    """
    def __init__(self, initial: float = 0.0, stiffness: float = 0.22, damping: float = 0.72):
        self.val = initial
        self.target = initial
        self.vel = 0.0
        self.stiffness = stiffness
        self.damping = damping

    def set(self, target: float, impulse: float = 0.0):
        self.target = target
        self.vel += impulse

    def snap(self, value: float):
        """Define valor e alvo imediatamente, zerando velocidade residual."""
        self.val = value
        self.target = value
        self.vel = 0.0

    def step(self) -> float:
        force = (self.target - self.val) * self.stiffness
        self.vel = (self.vel + force) * self.damping
        self.val += self.vel
        return self.val


def clamp01(t: float) -> float:
    """Restringe um valor ao intervalo [0.0, 1.0]."""
    return 0.0 if t <= 0.0 else (1.0 if t >= 1.0 else float(t))


def ease_out_quad(t: float) -> float:
    """Desaceleração quadrática suave."""
    u = clamp01(t)
    return 1.0 - (1.0 - u) * (1.0 - u)


def ease_out_cubic(t: float) -> float:
    """Desaceleração cúbica suave."""
    u = clamp01(t)
    inv = 1.0 - u
    return 1.0 - inv * inv * inv


def ease_out_back(t: float, overshoot: float = 1.45) -> float:
    """Curva com overshoot elástico suave no final (estilo pop orgânico)."""
    u = clamp01(t)
    c1 = overshoot
    c3 = c1 + 1.0
    m = u - 1.0
    return 1.0 + c3 * (m * m * m) + c1 * (m * m)

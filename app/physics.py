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

    def step(self) -> float:
        force = (self.target - self.val) * self.stiffness
        self.vel = (self.vel + force) * self.damping
        self.val += self.vel
        return self.val

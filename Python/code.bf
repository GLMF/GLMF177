;[>[+>]+[<]>-]>[>]<!< # L’initialisation
[                     # La boucle principale qui va construire le produit par la droite
  [                   # La boucle qui multiplie les deux nombres à droite
    >[>+>+<<-]
    >[<+>-]
    <<-
  ]
  >>>!
  [-<<<+>>>]<<        # On sauvegarde le produit à droite de la file de nombres restants
  [-]<<               # On supprime un résidu du produit sinon ça pourrit le produit suivant
]
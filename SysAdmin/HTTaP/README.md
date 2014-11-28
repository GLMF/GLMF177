# L'environnement POSIX du mini serveur embarqué en C
par Yann Guidon [Électronique, musique et informatique en folie^Wliberté]

---

Après les explications données précédemment sur le fond et la forme de HTTaP [1][2], nous pouvons commencer à coder notre petit serveur embarqué. Nous nous concentrons sur les fonctions de bas niveau essentielles au support du protocole HTTP/1.1. En effet, nous devons d'abord régler de nombreux détails en langage C, comme la configuration et les droits d'accès, en utilisant des techniques de codage communes aux autres types de serveurs TCP/IP. 

[1] Guidon Y., « Pourquoi utiliser HTTP pour interfacer des circuits numériques ? », GNU/Linux Magazine n°173, juillet 2014, p. 38.
[2] Guidon Y., « HTTaP : Un protocole de contrôle basé sur HTTP »,
GNU/Linux Magazine n°173, juillet 2014, p. 44.

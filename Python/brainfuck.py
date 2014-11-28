#!/usr/bin/python3
# -*- coding: utf-8 -*-

from sys import exit,argv

try:
  codebf=argv[1]
except IndexError:
  print("Usage: brainfuck.py code.bf [donnees.txt]")
  exit(0)

code=""
entree=False
commandes={",",".","+","-",">","<","[","]",";",":","#","!"}
sep={"\n","\t"," "}
nbo=0

with open(codebf,"r") as bf:
  for l in bf:
    for c in l.strip():
      if c not in commandes:
        continue
      if c=="#":
        break
      code=code+c
      if c in {",",";"}:
        entree=True
      if c=="[":
        nbo+=1
      elif c=="]":
        nbo-=1
        if nbo<0:
          print("Crochets incorrects.")
          exit(0)

if nbo!=0:
  print("Crochets incorrects.")
  exit(0)

if entree:
  try:
    entree=argv[2]
  except IndexError:
    print("Votre code nécessite un fichier de données.")
    exit(0)
  with open(entree,"r") as en:
    entrees=[]
    for l in en:
      entrees=entrees+list(l.strip())

p=0
dp=0
c=0
memoire=[0]

while p<len(code):
  c+=1
  if c>=10**6:
    print("Limite max. Votre code semble tourner en rond.")
    break
  elif code[p]==">":
    dp+=1
    if dp==len(memoire):
      memoire.append(0)
    p+=1
  elif code[p]=="<":
    dp-=1
    if dp==-1:
      memoire=[0]+memoire
      dp=0
    p+=1
  elif code[p]=="+":
    memoire[dp]+=1
    p+=1
  elif code[p]=="-":
    memoire[dp]-=1
    p+=1
  elif code[p]==".":
    print(chr(memoire[dp]),end="")
    p+=1
  elif code[p]==",":
    memoire[dp]=ord(entrees.pop(0))
    p+=1
  elif code[p]==":":
    print(memoire[dp],end=" ")
    p+=1
  elif code[p]==";":
    while entrees[0] in sep:
      entrees.pop(0)
    while entrees and entrees[0] not in sep:
      try:
        memoire[dp]*=10
        memoire[dp]+=int(entrees.pop(0))
      except ValueError:
        print("Entrée non numérique.")
        exit(0)
    p+=1
  elif code[p]=="[":
    if memoire[dp]!=0:
      p+=1
      continue
    nbo=1
    while nbo!=0:
      p+=1
      if code[p]=="[":
        nbo+=1
      if code[p]=="]":
        nbo-=1
    p+=1
  elif code[p]=="]":
    if memoire[dp]==0:
      p+=1
      continue
    nbf=1
    while nbf!=0:
      p-=1
      if code[p]=="]":
        nbf+=1
      if code[p]=="[":
        nbf-=1
  elif code[p]=="!":
    for i in range(len(memoire)):
      if i==dp:
        print("["+str(memoire[i])+"]",end=" ")
      else:
        print(memoire[i],end=" ")
    print()
    p+=1

print()
import os

def allRange(listx, start, end):
    if start == end:
        result = ""
        for i in listx:
            result += (str(i) + " ")
        # answer.txt is where your store previous answer
        # result.txt is where to output
        os.system("echo " + result + " | ./bomb answer.txt >> result.txt")
        os.system("echo " + result + ">> result.txt")
        # input()
    for m in range(start,end+1):
        listx[m],listx[start] = listx[start],listx[m]
        allRange(listx, start+1, end)
        listx[m],listx[start] = listx[start],listx[m]
list1 = [0,1,2,3,4,5,6]
allRange(list1, 0, 6)
# Introduction
Nowadays, social recommendation is an important task to enable online users to discover friends to follow, pages to read and items to buy. Big companies such as Facebook and Amazon heavily rely on high-quality recommendations to keep their users active. There exist numerous recommendation algorithms --- from the simple one based on common neighbor counting, to more complicated one based on deep learning models such as Graph Neural Networks. The common setup for those algorithms is that they represent the social network as a graph, and perform various operations on the nodes and edges.

This porject is a simple application to generate customized recommendations based on user queries. Specifically, consider that Facebook users in different countries want to follow new friends. They would send their queries to a Facebook server (i.e., main server) and receive the new friend suggestions as the reply from the same main server. Now since the Facebook social network is so large that it is impossible to store all the user information in a single machine. So we consider a distributed system design where the main server is further connected to many (in our case, two) backend servers. Each backend server stores the Facebook social network for different countries. For example, backend server A may store the user data of Canada and the US, and backend server B may store the data of Europe. Therefore, once the main server receives a user query, it decodes the query and further sends a request to the corresponding backend server. The backend server will search through its local data, identify the new friend to be recommended and reply to the main server. Finally, the main server will reply to the user to conclude the process.

# Structures


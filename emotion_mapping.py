# Sydney Savarese
# Empower Lab, Thayer School of Engineering at Dartmouth

# LSA Model Builder

# Performs Latent Semantic Analysis on a corpus of documents, determines the optimal number of topics using coherence values, 
# and saves word occurrences per document in a CSV file for further analysis.

#import modules
import os.path
from gensim import corpora
from gensim.models import LsiModel
import nltk
nltk.download('stopwords')
from nltk.tokenize import RegexpTokenizer
from nltk.corpus import stopwords
from nltk.stem.porter import PorterStemmer
from gensim.models.coherencemodel import CoherenceModel
import matplotlib.pyplot as plt
import csv
import pandas as pd




def load_data(path,file_name):
    """
    Input  : path and file_name
    Purpose: loading text file
    Output : list of paragraphs/documents and
             title(initial 100 words considred as title of document)
    """
    documents_list = []
    titles=[]
    with open( os.path.join(path, file_name) ,"r") as fin:
        for line in fin.readlines():
            text = line.strip()
            documents_list.append(text)
    print("Total Number of Documents:",len(documents_list))
    titles.append( text[0:min(len(text),100)] )
    return documents_list,titles

def preprocess_data(doc_set):
    """
    Input  : document list
    Purpose: preprocess text (tokenize, removing stopwords, and stemming)
    Output : preprocessed text
    """
    # initialize regex tokenizer
    tokenizer = RegexpTokenizer(r'\w+')
    # create English stop words list
    en_stop = set(stopwords.words('english'))
    # Create p_stemmer of class PorterStemmer
    p_stemmer = PorterStemmer()
    # list for tokenized documents in loop
    texts = []
    # loop through document list
    for i in doc_set:
        print("i: "+i)
        # clean and tokenize document string
        raw = i.lower()
        tokens = tokenizer.tokenize(raw)
        # remove stop words from tokens
        stopped_tokens = [i for i in tokens if not i in en_stop]
        # stem tokens
        stemmed_tokens = [p_stemmer.stem(i) for i in stopped_tokens]
        # add tokens to list
        texts.append(stemmed_tokens)
    return texts

def prepare_corpus(doc_clean):
    """
    Input  : clean document
    Purpose: create term dictionary of our courpus and Converting list of documents (corpus) into Document Term Matrix
    Output : term dictionary and Document Term Matrix
    """
    # Creating the term dictionary of our courpus, where every unique term is assigned an index. dictionary = corpora.Dictionary(doc_clean)
    dictionary = corpora.Dictionary(doc_clean)
    # Converting list of documents (corpus) into Document Term Matrix using dictionary prepared above.
    doc_term_matrix = [dictionary.doc2bow(doc) for doc in doc_clean]
    # generate LDA model
    #print(dictionary)
    return dictionary,doc_term_matrix

def create_gensim_lsa_model(doc_clean,number_of_topics,words):
    """
    Input  : clean document, number of topics and number of words associated with each topic
    Purpose: create LSA model using gensim
    Output : return LSA model
    """
    dictionary,doc_term_matrix=prepare_corpus(doc_clean)
    # generate LSA model
    lsamodel = LsiModel(doc_term_matrix, num_topics=number_of_topics, id2word = dictionary)  # train model
    #print(lsamodel.print_topics(num_topics=number_of_topics, num_words=words))
    return lsamodel

#determine number of topics
def compute_coherence_values(dictionary, doc_term_matrix, doc_clean, stop, start=2, step=3):
    """
    Input   : dictionary : Gensim dictionary
              corpus : Gensim corpus
              texts : List of input texts
              stop : Max num of topics
    purpose : Compute c_v coherence for various number of topics
    Output  : model_list : List of LSA topic models
              coherence_values : Coherence values corresponding to the LDA model with respective number of topics
    """
    coherence_values = []
    model_list = []
    for num_topics in range(start, stop, step):
        # generate LSA model
        model = LsiModel(doc_term_matrix, num_topics, id2word = dictionary)  # train model
        model_list.append(model)
        coherencemodel = CoherenceModel(model=model, texts=doc_clean, dictionary=dictionary, coherence='c_v')
        coherence_values.append(coherencemodel.get_coherence())
    #print("Model list: ")
    #for x in range(len(model_list)):
            #print (model_list[x])
    #print("coherence values: ")
    #for x in range(len(model_list)):
            #print (coherence_values[x])
    return model_list, coherence_values

#main method
path = '/content/drive/Shareddrives/Empower Lab/Projects/Sprout/NLP'
docsList, titles = load_data(path, 'Test3.txt')
cleanData = preprocess_data(docsList)
dict, matrix = prepare_corpus(cleanData)
modelList, coVal = compute_coherence_values(dict, matrix, cleanData, 7, start =2, step=3)
max = 0;
i = 0
#calculate numTopics
for val in coVal:
    if val>max:
        max = val
        model = modelList[i]
        i = i +1
numTopics = model.get_topics().size
print(dict)
print(matrix)
total_words = len(dict)
words = list(dict.token2id.keys())
print(words)
numWords = total_words/numTopics
model=create_gensim_lsa_model(cleanData,numTopics,numWords)
row = 1

#create csv to document occurences per word
with open('wordInstances.csv', 'w', newline='') as file:
    writer = csv.writer(file)
    writer.writerow(['word index', 'word', 'occurrences'])

#loop through all pairs in matrix
#if first index is new, create new row in csv
#if it's not new, add the the second value associated with it to the corresponding existing occurance column value
#ensure that csv is a dataframe
#pandas ilock
wordIn = 0
prevMax = -1
print(len(words))
for s in matrix:
  for pair in s: #loops through (word index, occurrences per line) sets
    if pair:
        if pair[0] > prevMax: #if word index is not already in csv
            print("Previous max: ")
            print(prevMax)
            prevMax = pair[0]
            print("Word index: ")
            print(pair[0])

            # Append a new row to the CSV file
            with open('wordInstances.csv', 'a', newline='') as file:
                writer = csv.writer(file)
                print("WordIn count: ")
                print(wordIn)
                writer.writerow([pair[0], words[wordIn], pair[1]])
            wordIn += 1

        else:
            # Read the content into a DataFrame
            df = pd.read_csv('wordInstances.csv')

            # Increment the value in the 'occurrences' column
            df['occurrences'] = df['occurrences'] + pair[1]

            # Write the modified DataFrame back to the file
            df.to_csv('wordInstances.csv', index=False)
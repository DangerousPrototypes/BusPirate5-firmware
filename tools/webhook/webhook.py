############################################
#   A script to check git for new commits, 
#   run make, package up the output
#   and POST to discourse forum.
#
#
#   Used at:
#       http://forum.buspirate.com/
#
##############################################
import sys
import subprocess
import shlex
import json
import time
import os
import threading
from os.path import basename
#import argparse
import base64
import requests
from pprint import pprint
from zipfile import ZipFile, ZIP_DEFLATED
from dotenv import dotenv_values

# for the webhook
from github_webhook import Webhook
from flask import Flask

import argparse

# Parse command line arguments
parser = argparse.ArgumentParser(description='Webhook script with optional test mode.')
parser.add_argument('--test', action='store_true', help='Enable test mode')
args = parser.parse_args()

class autoBuild:
    FIXED_FILES = [
        {"file": "tools/webhook/README.TXT", "zip_file_location": ""},
        {"file": "tools/webhook/WARNING.TXT", "zip_file_location": ""},
        {"file": "tools/webhook/BACKUP!!.TXT", "zip_file_location":""}
    ]

    # Shared build definitions: rp2040 + rp2350
    BUILDS = [
        {
            "work_dir":"/build",
            "make_commands":["cmake .. --fresh -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DGIT_COMMIT_HASH=%%GIT_COMMIT_HASH%% -DBP_PICO_PLATFORM=rp2040", "make"],
            "output_dir":"/build/src",
            "output_file":[{"file":"bus_pirate5_rev8.uf2","zip_file_location":"attic"},
                            {"file":"bus_pirate5_rev10.uf2", "zip_file_location":""}],
        },
        {
            "work_dir":"/build",
            "make_commands":["cmake .. --fresh -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DGIT_COMMIT_HASH=%%GIT_COMMIT_HASH%% -DBP_PICO_PLATFORM=rp2350", "make"],
            "output_dir":"/build/src",
            "output_file":[{"file":"bus_pirate5_xl.uf2","zip_file_location":"attic"},
                            {"file":"bus_pirate6.uf2", "zip_file_location":""}],
        }
    ]

    @staticmethod
    def _make_task(branch, pull_dir, zip_prefix, topic_id, topic_text):
        return {
            "test": True,
            "git_repo":"https://github.com/DangerousPrototypes/BusPirate5-firmware.git",
            "git_branch":branch,
            "git_pull_dir":pull_dir,
            "zip_name":zip_prefix,
            "logs_working_dir":"/build",
            "topic_id": topic_id,
            "topic_text": topic_text,
            "builds": autoBuild.BUILDS,
        }

    def __init__(self, api_user, api_key, api_url):
        self.api_user = api_user
        self.api_key = api_key
        self.api_url = api_url
        #
        # Each TASK runs git pull, then executes one or more BUILDS
        #
        self.tasks={
            "refs/heads/main": self._make_task(
                "main", "/root/bp5-main",
                "/root/webhook/ci-buspirate-main-",
                20, "**WARNING**: New flash driver in use. It should be backwards compatible, but **BACKUP THE INTERNAL STORAGE** prior to upgrade if you value the contents.\n\nHere's a fresh compile from 'main':"),
            "refs/heads/refactor2026-2": self._make_task(
                "refactor2026-2", "/root/bp5-refact",
                "/root/webhook/ci-buspirate-experimental-",
                1417, "Here's a fresh compile from 'refactor2026-2':"),
        }
        for fixed_file in autoBuild.FIXED_FILES:
            src_path = os.path.join(self.tasks['refs/heads/main']['git_pull_dir'], fixed_file['file'])
            print(src_path)
            if os.path.exists(src_path):
                print("exists")
            else:
                print("not exists")

    def runTasks(self, ref, push_data):
        if(ref in self.tasks):
            print('Preparing build report')
            if not os.path.isdir(self.tasks[ref]['git_pull_dir']):
                self.clone(self.tasks[ref])
            pull_result=self.pull(self.tasks[ref])
            self.tasks[ref]['pull_result']=pull_result
            if self.tasks[ref]['test'] is not False or pull_result['hashlong'] != pull_result['newhashlong']:
                self.tasks[ref]['push_data']=push_data
                self.tasks[ref]['timestamp']={}
                self.tasks[ref]['timestamp']['start']=time.time()
                self.tasks[ref]['error']=False
                for build in self.tasks[ref]['builds']:
                    build['make_result']=self.make(build, pull_result, self.tasks[ref]['git_pull_dir'])
                    if(build['make_result']['error']): self.tasks[ref]['error']=True
                self.tasks[ref]['package_output_file_name']=self.package(self.tasks[ref])
                self.tasks[ref]['timestamp']['stop']=time.time()
                self.post(self.tasks[ref])
                self.clean(self.tasks[ref])
            #pprint(task)
            #pprint(task['work_dir'])
        else:
            print("No tasks for this ref: "+ref)

    def clone(self, task):
        #git clone
        result={}
        print('Cloning '+task['git_repo']+' to '+task['git_pull_dir'])
        result['gitoutput']=subprocess.run(['git', 'clone', task['git_repo'], task['git_pull_dir']], check=True, capture_output=True, text=True).stdout
        print(result['gitoutput'])
        #check out branch
        print('Checking out '+task['git_branch'])
        result['gitoutput']=result['gitoutput']+subprocess.run(['git', 'checkout', task['git_branch']], cwd=task['git_pull_dir'], check=True, capture_output=True, text=True).stdout
        print(result['gitoutput'])       
        return result
    
    def pull(self, task):
        result={}
        #git log --pretty=format:'%h' -n 1 #or H for long hash
        #current hash
        result['hashshort'] = subprocess.run(['git', 'log', '--pretty=format:%h', '-n', '1'], cwd=task['git_pull_dir'], check=True, capture_output=True, text=True).stdout.strip()
        result['hashlong'] = subprocess.run(['git', 'log', '--pretty=format:%H', '-n', '1'], cwd=task['git_pull_dir'], check=True, capture_output=True, text=True).stdout.strip()
        #git pull
        result['gitoutput']=subprocess.run(['git', 'pull'], cwd=task['git_pull_dir'], check=True, capture_output=True, text=True).stdout
        #new hash
        result['newhashshort'] = subprocess.run(['git', 'log', '--pretty=format:%h', '-n', '1'], cwd=task['git_pull_dir'], check=True, capture_output=True, text=True).stdout.strip()
        result['newhashlong'] = subprocess.run(['git', 'log', '--pretty=format:%H', '-n', '1'], cwd=task['git_pull_dir'], check=True, capture_output=True, text=True).stdout.strip()

        print(result['hashlong'])
        print(result['gitoutput'])
        print(result['newhashlong'])
        return result
        
    def make(self, build, pull_result, base_path):
        result={}
        #pprint(build)
        result['makeoutput']=''
        result['error']=False
        for make_command in build['make_commands']:
            work_dir=os.path.join(base_path, build['work_dir'].lstrip('/'))
            if not os.path.isdir(work_dir):
                print('Creating directory '+work_dir)
                os.makedirs(work_dir)
            command_str=make_command.replace("%%GIT_COMMIT_HASH%%",pull_result['newhashshort'])
            result['makeoutput']=result['makeoutput'] + "\n\n\n" +command_str
            try:
                result['makeoutput']=result['makeoutput'] + "\n" +subprocess.run(shlex.split(command_str), cwd=work_dir, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True).stdout
            except subprocess.CalledProcessError as e:
                #print("command '"+e.cmd+"' return with error (code "+e.returncode.decode()+"): "+e.output) 
                pprint(e.returncode)
                #pprint(stderr)
                result['makeoutput']=result['makeoutput'] + "\n" +str(e.output)
                result['error']=True
                pprint(result['makeoutput'])
                return result
            subprocess.run(['make', 'clean'], cwd=work_dir, check=True, capture_output=True, text=True)
        return result
    
    def package(self, task):
        log_dir = os.path.join(task['git_pull_dir'], task['logs_working_dir'].lstrip('/'))
        filename=task['zip_name']+task['pull_result']['newhashshort']+'.zip'

        with open(os.path.join(log_dir, "git.log"), "w") as git_log:
            git_log.write(task['pull_result']['gitoutput'])

        with open(os.path.join(log_dir, "make.log"), "w") as makelog_file, \
             ZipFile(filename, 'w', compression=ZIP_DEFLATED, compresslevel=9) as zip:
            for build in task['builds']:  
                makelog_file.write(build['make_result']['makeoutput']+"\n\n\n")              
                for output_file in build['output_file']:
                    src_path = os.path.join(task['git_pull_dir'], build['output_dir'].lstrip('/'), output_file['file'])
                    if os.path.exists(src_path):
                        if output_file['zip_file_location'] != "":
                            zip.write(src_path, os.path.join(output_file['zip_file_location'],output_file['file']))
                        else:
                            zip.write(src_path, output_file['file'])
                        print(src_path)
            for fixed_file in autoBuild.FIXED_FILES:
                src_path = os.path.join(task['git_pull_dir'], fixed_file['file'])
                print(src_path)
                if os.path.exists(src_path):
                    zip_location = fixed_file['zip_file_location']
                    if zip_location:
                        zip.write(src_path, os.path.join(zip_location, os.path.basename(fixed_file['file'])))
                    else:
                        zip.write(src_path, os.path.basename(fixed_file['file']))
                    print(src_path)
            makelog_file.flush()
            zip.write(os.path.join(log_dir, "git.log"), "git.log")
            zip.write(os.path.join(log_dir, "make.log"), "make.log")
        return filename  

    def post(self, task):
        filename=task['package_output_file_name']

        headers = {
            "Accept": "application/json; charset=utf-8",
            "Api-Key": self.api_key,
            "Api-Username": self.api_user,
        }
        params={
            "type":'composer',
            "synchronous":1,
        }
        url=self.api_url+'uploads.json'
        with open(filename, "rb") as f:
            response = requests.post(url,headers=headers, data=params, files={"file": f})
        print("Status Code", response.status_code)
        if not response.ok:
            print(f"Discourse API error {response.status_code}: {response.text}")
            return
        
        json=response.json()

        # Download link at the top
        raw=":arrow_down: :arrow_down: :arrow_down: **["+str(filename.split("/")[-1])+"|attachment]("+json['short_url']+")** :arrow_down: :arrow_down: :arrow_down:\n\n"

        if(task['error']):
            raw+="**Build Failed!** Logs are attached. Happy debugging!\n\n"
        else:
            raw+=task['topic_text']+"\n\n"

        # Build commit list
        commits = task['push_data'].get('commits', [])
        compare_url = task['push_data'].get('compare', '')
        max_shown = 10
        for commit in commits[:max_shown]:
            sha_short = commit['id'][:7]
            first_line = commit['message'].split('\n')[0]
            raw += "- [`" + sha_short + "`](" + commit['url'] + "): " + first_line + "\n"
        if len(commits) > max_shown:
            remaining = len(commits) - max_shown
            if compare_url:
                raw += "- ...and " + str(remaining) + " more commits ([view all](" + compare_url + "))\n"
            else:
                raw += "- ...and " + str(remaining) + " more commits\n"
        raw += "\n"

        raw=raw+"Build time: "+str(round(task['timestamp']['stop']-task['timestamp']['start'],2)) +" seconds"
        if compare_url:
            raw += " | [compare](" + compare_url + ")"
        raw += "\n"

        params={
            "raw":raw,
            "topic_id":task['topic_id'],
        }
        url=self.api_url+'posts.json'
        response = requests.post(url,headers=headers, data=params)    
        print("Status Code", response.status_code)
        if not response.ok:
            print(f"Discourse API error {response.status_code}: {response.text}")

    def clean(self, task):
        for build in task['builds']:
            #subprocess.check_output('cd '+task['git_pull_dir']+build['work_dir']+' && make clean', shell=True)
            print('Clean disabled')

class discourseBot:
    def __init__(self, api_user, api_key, api_url, api_chat_key):
        self.api_user=api_user
        self.api_key=api_key
        self.api_url=api_url
        self.api_chat_key=api_chat_key
        self.headers = {
            "Accept": "application/json; charset=utf-8",
            "Api-Key": self.api_key,
            "Api-Username": self.api_user,
        }
    
    def check_topic(self, external_id):
        print("Checking for external_id " + str(external_id))
        response = requests.get(self.api_url+'t/external_id/'+external_id+'.json',headers=self.headers)
        print("Status Code", response.status_code)
        if response.status_code == 200:
            print("Found topic_id "+ str(response.json()["post_stream"]["posts"][0]["topic_id"]))
            return response.json()["post_stream"]["posts"][0]["topic_id"]
        else:
            print("external_id does not exist")
            return 0

    def create_topic(self, title, category_id, raw, external_id):
        print("Creating new topic")
        params={
            "title":title,
            "category": category_id,
            "raw":raw,
            "external_id":external_id,
        }
        response = requests.post(self.api_url+'posts.json',headers=self.headers, data=params)
        print("Status Code", response.status_code)
        if not response.ok:
            print(f"Discourse API error {response.status_code}: {response.text}")
            return 0
        topic_id = response.json()["topic_id"]
        print("New topic id: ", topic_id)
        return topic_id

    def create_post(self, topic_id, raw):
        print("Creating new comment")
        params={
            "raw":raw,
            "topic_id":topic_id,
        }
        response = requests.post(self.api_url+'posts.json',headers=self.headers, data=params)    
        print("Status Code", response.status_code)
        if not response.ok:
            print(f"Discourse API error {response.status_code}: {response.text}")

    def send_to_chat(self, text):
        print("Posting to live chat")
        params={
            "text":text
        }
        response = requests.post(self.api_url+'chat/hooks/'+self.api_chat_key+'.json', data=params)
        print("Status Code", response.status_code) 


class pullRequest:
    def __init__(self, dbot, category_id):
        self.dbot = dbot
        self.category_id = category_id

    def get_external_id(self, external_id):
        return 'ghi-v1-'+str(external_id)
    def opened(self, data): 
        #new push, create new topic
        if data["action"] == "opened":
            print("Pull Request opened event")
            topic_id=self.dbot.check_topic(self.get_external_id(data["pull_request"]["number"]))

            #duplicate
            if(topic_id != 0):
                return

            raw="New Pull Request: ["+data["pull_request"]["title"]+"]("+data["pull_request"]["html_url"]+")\n"
            raw=raw+"**```"+data["pull_request"]["head"]["repo"]["full_name"]+"```** --> **```"+data["pull_request"]["base"]["repo"]["full_name"]+"```**\n\n"
            raw=raw+data["pull_request"]["body"]+"\n\n"
            raw=raw+"Created by: ```"+data["pull_request"]["user"]["login"]+"```\n\n"
            title="Pull Request #" + str(data["pull_request"]["number"]) +": " +data["pull_request"]["title"]
            topic_id = self.dbot.create_topic(title, self.category_id, raw, self.get_external_id(data["pull_request"]["number"]))
            #post to live chat
            self.dbot.send_to_chat("*New pull request:* ["+data["pull_request"]["title"]+"]("+data["pull_request"]["html_url"]+")")

    def reply(self, data):
        if data["action"] == "created":
            print("Pull Request reply event")
            topic_id=self.dbot.check_topic(self.get_external_id(data["issue"]["number"]))

            if topic_id != 0 :
                raw=data["comment"]["body"]+"\n\n"
                raw=raw+"*Comment by: ["+data["comment"]["user"]["login"]+"]("+data["comment"]["html_url"]+")*\n\n"
                self.dbot.create_post(topic_id, raw)
                #post to live chat
                #self.dbot.send_to_chat("*Pull request reply:* ["+data["issue"]["title"]+"]("+data["issue"]["html_url"]+")")


#new class that receives issue events, searches a database to see if we already have it, and if not, posts a new topic otherwise posts a reply
class issues:
    def __init__(self, dbot, category_id):
        self.dbot = dbot
        self.category_id = category_id

    def get_external_id(self, external_id):
        return 'ghi-v1-'+str(external_id)
    
    def process(self, data):
        if data["action"] == "opened" or data["action"] == "created":
            print("Issue event")
            print(data["issue"]["html_url"])
            print(data["issue"]["title"])
            topic_id=self.dbot.check_topic(self.get_external_id(data["issue"]["number"]))

            #duplicate
            if topic_id != 0 and data["action"] == "opened":
                return

            #new issue or orphaned comment, create new topic
            if topic_id == 0:
                raw="["+data["issue"]["title"]+"]("+data["issue"]["html_url"]+"):\n"
                raw=raw+data["issue"]["body"]+"\n\n"
                raw=raw+"*Issue opened by: "+data["issue"]["user"]["login"]+"*\n\n"
                title="Issue #" + str(data["issue"]["number"]) +": " +data["issue"]["title"]
                topic_id = self.dbot.create_topic(title, self.category_id, raw, self.get_external_id(data["issue"]["number"]))
                #post to live chat
                self.dbot.send_to_chat("*New issue:* ["+data["issue"]["title"]+"]("+data["issue"]["html_url"]+")")

            #we failed to query or create the new topic
            #or this is a new issue
            if topic_id != 0 and data["action"] == "created":
                raw=data["comment"]["body"]+"\n\n"
                raw=raw+"*Bot-copy from [GitHub comment]("+data["comment"]["html_url"]+") by: ["+data["comment"]["user"]["login"]+"]("+data["comment"]["html_url"]+")*\n\n"
                self.dbot.create_post(topic_id, raw)
                #post to live chat
                #self.dbot.send_to_chat("*Issue reply:* ["+data["issue"]["title"]+"]("+data["issue"]["html_url"]+")")


config = dotenv_values(".env")
if args.test:
    print("Test mode enabled")
    # Add any test mode specific behavior here
    #config = dotenv_values(".env")
    ab=autoBuild(config['API_USER'], config['API_KEY'], config['API_URL'])
    ab.runTasks('refs/heads/main', {"commits": [], "compare": "", "head_commit": None})    
else:
    print("Normal mode")
    app = Flask(__name__)  # Standard Flask app
    webhook = Webhook(app, secret=config['WEBHOOK_SECRET']) # Defines '/postreceive' endpoint

    dbot = discourseBot(config['API_USER'], config['API_KEY'], config['API_URL'], config['API_CHAT_KEY'])
    ab = autoBuild(config['API_USER'], config['API_KEY'], config['API_URL'])
    pr = pullRequest(dbot, 8)
    iss = issues(dbot, 8)

    #@app.route("/")        # Standard Flask endpoint
    #def hello_world():
    #    return "Hello, World!"

    @webhook.hook(event_type="pull_request")
    def on_pull_request(data):
        pr.opened(data)

    @webhook.hook(event_type="push")
    def on_push(data):
        push_data = {
            "commits": data.get("commits", []),
            "compare": data.get("compare", ""),
            "head_commit": data.get("head_commit"),
        }
        ref = data["ref"]
        threading.Thread(target=ab.runTasks, args=(ref, push_data), daemon=True).start()  

    @webhook.hook(event_type="issues")  
    def on_issues(data):
        #print(data["action"])
        #print("Got issues with: {0}".format(data))
        iss.process(data)     

    @webhook.hook(event_type="issue_comment")
    def on_issue_comment(data):
        #print(data["action"])
        #print("Got issue_comment with: {0}".format(data))
        if "pull_request" in data['issue']:
            pr.reply(data)
        else:      
            iss.process(data)                                         
                
    if __name__ == "__main__":
        app.run(host="0.0.0.0", port=80)

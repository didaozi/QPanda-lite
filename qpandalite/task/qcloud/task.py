import time
import traceback
from typing import List, Union
import requests
from pathlib import Path
import os
import json
from requests.packages.urllib3.exceptions import InsecureRequestWarning
import warnings
from json.decoder import JSONDecodeError
import bz2

from qpandalite.task.task_utils import *

requests.packages.urllib3.disable_warnings(InsecureRequestWarning)


# Define default values for your configuration parameters
default_online_config = {
    'apitoken': 'default_api_token',
    'submit_url': 'default_submit_url',
    'query_url': 'default_query_url',
    'task_group_size': 'default_task_group_size'
}

# Only attempt to read the config file if we're not generating docs
if os.getenv('SPHINX_DOC_GEN') != '1':
    try:
        with open('originq_cloud_config.json', 'r') as fp:
            default_online_config = json.load(fp)
    except FileNotFoundError as e:
        raise ImportError('Import origin qcloud backend failed.\n'
                          'originq_cloud_config.json is not found. '
                          'It should be always placed at current working directory (cwd).')
    except JSONDecodeError as e:
        raise ImportError('Import origin qcloud  backend failed.\n'
                          'Cannot load json from the originq_cloud_config.json. '
                          'Please check the content.')
    except Exception as e:
        raise ImportError('Import origin qcloud  failed.\n'
                          'Unknown import error.'
                          '\n===== Original exception ======\n'
                          f'{traceback.format_exc()}')

    try:
        default_apitoken = default_online_config['apitoken']
        default_submit_url = default_online_config['submit_url']
        default_query_url = default_online_config['query_url']
        default_task_group_size = default_online_config['task_group_size']
    except KeyError as e:
        raise ImportError('Import origin qcloud  backend failed.\n'
                          'originq_cloud_config.json does not exist such a key.'
                          '\n===== Original exception ======\n'
                          f'{traceback.format_exc()}')

# Now you can safely use the configuration values:
default_token = default_online_config['apitoken']
default_submit_url = default_online_config['submit_url']
default_query_url = default_online_config['query_url']
default_task_group_size = default_online_config['task_group_size']


def parse_response_body(response_body):
    '''Parse response body (in query_by_taskid)

    Args:
        response_body (Dict): The response body returned by the server.

    Returns:
        Dict: The parsed dict containing these fields:
            - taskid
            - taskname
            - status
            - compiled_prog (?)
            - result (not always)
    '''

    ret = dict()
    recv_dict = response_body

    if not recv_dict["success"]:
        message = recv_dict["message"]
        raise Exception(f"query task error : {message}")

    result_list = recv_dict["obj"]["qcodeTaskNewVo"]["taskResultList"][0]

    ret['taskid'] = result_list['rTaskId']
    ret['taskname'] = result_list['taskName']

    task_status = result_list['taskState']
    if task_status == '3':
        # successfully finished !
        ret['status'] = 'success'

        # task_result
        task_result = result_list['taskResult']
        try:
            task_result = json.loads(task_result)
        except json.decoder.JSONDecodeError as e:
            raise RuntimeError('Error when parsing the response task_result. '
                               f'task_result = {task_result}')
        ret['result'] = task_result

        # compiled_prog = response_body['without_compensate_prog']
        # print(type(compiled_prog))
        # ret['compiled_prog'] = json.loads(compiled_prog)
        # timelines = response_body['compile_output_prog']
        # timelines_ret = []
        # for timeline in timelines:
        #     timelines_ret.append(json.loads(timeline))
        # ret['timeline'] = timelines_ret
        return ret
    elif task_status == '4':
        ret['status'] = 'failed'
        ret['result'] = {'errcode': result_list['errorDetail'], 'errinfo': result_list['errorMessage']}

        return ret
    else:
        ret['status'] = 'running'
        return ret


def query_by_taskid_single(taskid: str, url=default_query_url, **kwargs):
    """
    Query circuit status by taskid (Async). This function will return without waiting.

    Args:
        taskid (str): The taskid.
        url (str, optional): The querying URL. Defaults to default_query_url.
        retry (int): The number of repetitions of failed submissions.

    Raises:
        ValueError: Taskid invalid.
        ValueError: URL invalid.
        RuntimeError: Error when querying.

    Returns:
        Dict[str, dict]: The status and the result
            status : success | failed | running
            result (when success): List[Dict[str,list]]
            result (when failed): {'errcode': str, 'errinfo': str}
            result (when running): N/A
    """

    if not taskid: raise ValueError('Task id ??')
    if not url: raise ValueError('URL invalid.')

    headers = dict()
    headers['origin-language'] = 'en'
    headers['Connection'] = 'keep-alive'
    headers['Content-Type'] = 'application/json;charset=UTF-8'
    headers['Authorization'] = 'oqcs_auth=' + default_token

    # construct request_body for task query
    request_body = dict()
    request_body['apiKey'] = default_token
    request_body['taskId'] = taskid

    retry = kwargs.get('retry', 5)

    while True:
        try:
            response = requests.post(url=url,
                                     headers=headers,
                                     json=request_body,
                                     verify=False,
                                     timeout=30)
            break
        except:
            if retry > 0:
                retry -= 1
                print(f'submit failed. Retry remains {retry} times.')
                time.sleep(1)
            else:
                print(f'Retry count exhausted.')
                raise e

    status_code = response.status_code
    if status_code != 200:
        raise RuntimeError(f'Error in query_by_taskid. '
                           'The returned status code is not 200.'
                           f' Response: {response.text}')

    if response.content[:2] == b'BZ':
        text = bz2.decompress(response.content)
    else:
        text = response.text

    response_body = json.loads(text)
    taskinfo = parse_response_body(response_body)

    return taskinfo


def query_by_taskid(taskid: Union[List[str], str],
                    url=default_query_url,
                    **kwargs):
    '''Query circuit status by taskid (Async). This function will return without waiting.

    Args:
        taskid (str): The taskid.
        url (str, optional): The querying URL. Defaults to default_query_url.

    Raises:
        ValueError: Taskid invalid.
        ValueError: URL invalid.
        RuntimeError: Error when querying.

    Returns:
        Dict[str, dict]: The status and the result
            status : success | failed | running
            result (when success): List[Dict[str,list]]
            result (when failed): {'errcode': str, 'errinfo': str}
            result (when running): N/A
    '''
    if not taskid: raise ValueError('Task id ??')

    if isinstance(taskid, list):
        taskinfo = dict()
        taskinfo['status'] = 'success'
        taskinfo['result'] = []
        for taskid_i in taskid:
            taskinfo_i = query_by_taskid_single(taskid_i, url)
            if taskinfo_i['status'] == 'failed':
                # if any task is failed, then this group is failed.
                taskinfo['status'] = 'failed'
                break
            elif taskinfo_i['status'] == 'running':
                # if any task is running, then set to running
                taskinfo['status'] = 'running'
            if taskinfo_i['status'] == 'success':
                if taskinfo['status'] == 'success':
                    # update if task is successfully finished (so far)
                    taskinfo['result'].extend(taskinfo_i['result'])

    elif isinstance(taskid, str):
        taskinfo = query_by_taskid_single(taskid, url)
    else:
        raise ValueError('Invalid Taskid')

    return taskinfo


def query_by_taskid_sync(taskid,
                         interval=2.0,
                         timeout=60.0,
                         retry=5,
                         url=default_query_url,
                         **kwargs):
    '''Query circuit status by taskid (synchronous version), it will wait until the task finished.

    Args:
        taskid (str): The taskid.
        interval (float): Interval time between two queries. (in seconds)
        url (str, optional): The querying URL. Defaults to default_query_url.

    Raises:
        RuntimeError: Taskid invalid.
        RuntimeError: URL invalid.
        TimeoutError: Timeout reached

    Returns:
        Dict[str, dict]: The status and the result
            status : success | failed | running
            result (when success): List[Dict[str,list]]
            result (when failed): {'errcode': str, 'errinfo': str}
            result (when running): N/A
    '''
    starttime = time.time()
    while True:
        try:
            now = time.time()
            if now - starttime > timeout:
                raise TimeoutError(f'Reach the maximum timeout.')

            time.sleep(interval)

            taskinfo = query_by_taskid(taskid, url)
            if taskinfo['status'] == 'running':
                continue
            if taskinfo['status'] == 'success':
                result = taskinfo['result']
                return result
            if taskinfo['status'] == 'failed':
                errorinfo = taskinfo['result']
                raise RuntimeError(f'Failed to execute, errorinfo = {errorinfo}')
        except RuntimeError as e:
            if retry > 0:
                retry -= 1
                print(f'Query failed. Retry remains {retry} times.')
            else:
                print(f'Retry count exhausted.')
                raise e


def _submit_task_group(circuits=None,
                       task_name=None,
                       tasktype=None,
                       chip_id=72,
                       shots=1000,
                       circuit_optimize=True,
                       measurement_amend=False,
                       auto_mapping=False,
                       compile_only=False,
                       specified_block=None,
                       url=default_submit_url,
                       savepath=Path.cwd() / 'online_info'
                       ):
    '''submit taskgroup

    Args:
        circuits (List[str]): A list of quantum circuits to be submitted.
        task_name (str, optional): The name of the task. Defaults to None.
        tasktype (int): The tasktype. Defaults to None. (Note: reserved field.)
        chip_id (int, optional): The chip id used to identify the quantum chip. Defaults to 72.
        shots (int, optional): Number of shots for every circuit. Defaults to 1000.
        circuit_optimize (bool, optional): Automatically optimize and transpile the circuit. Defaults to True.
        measurement_amend (bool, optional): Amend the measurement result using an internal algorithm. Defaults to True.
        auto_mapping (bool, optional): Automatically select the mapping. Defaults to False.
        compile_only (bool, optional): Only compile time sequence data, without really executing it. Defaults to False.
        specified_block (int, optional): The specified block on chip. Defaults to None. (Note: reserved field.)
        url (str, optional): The URL for submitting the task. Defaults to default_submit_url.
        savepath (str, optional): str. Defaults to Path.cwd()/'online_info'. If None, it will not save the task info.

    Raises:
        ValueError: Circuit not input
        RuntimeError: Circuit number exceeds the default_task_group_size
        RuntimeError: Error when submitting the task

    Returns:
        int: The taskid of this taskgroup
    '''
    if not circuits: raise ValueError('circuit ??')
    if len(circuits) > default_task_group_size:
        groups = []
        group = []
        for circuit in circuits:
            if len(group) >= default_task_group_size:
                groups.append(group)
                group = []
            group.append(circuit)
        if group:
            groups.append(group)

        return [_submit_task_group(group,
                                   '{}_{}'.format(task_name, i),
                                   tasktype,
                                   chip_id,
                                   shots,
                                   circuit_optimize,
                                   measurement_amend,
                                   auto_mapping,
                                   compile_only,
                                   specified_block,
                                   url,
                                   savepath) for i, group in enumerate(groups)]

        # raise RuntimeError(f'Circuit group size too large. '
        #                    f'(Expect: <= {default_task_group_size}, Get: {len(circuits)})')

    # construct request_body for task query
    headers = dict()
    headers['origin-language'] = 'en'
    headers['Connection'] = 'keep-alive'
    headers['Content-Type'] = 'application/json;charset=UTF-8'
    headers['Authorization'] = 'oqcs_auth=' + default_token

    request_body = dict()
    request_body['qmachineType'] = tasktype if tasktype is not None else 5
    request_body['qprogArr'] = circuits
    request_body['taskFrom'] = 4  # what's this
    request_body['chipId'] = chip_id
    request_body['shot'] = shots
    request_body['isAmend'] = 1 if measurement_amend else 0
    request_body['mappingFlag'] = 1 if auto_mapping else 0
    request_body['circuitOptimization'] = 1 if circuit_optimize else 0
    request_body['compileLevel'] = 3

    response = requests.post(url=url,
                             headers=headers,
                             json=request_body,
                             verify=False,
                             timeout=10)
    status_code = response.status_code
    if status_code != 200:
        raise RuntimeError(f'Error in submit_task. The returned status code is not 200.'
                           f' Response: {response.text}')

    try:
        text = response.text
        response_body = json.loads(text)
        # task_status = response_body['taskState']
        task_id = response_body['obj']['taskId']
    except Exception as e:
        raise RuntimeError(f'Error in submit_task. The response body is corrupted. '
                           f'Response body: {response_body}')

    return task_id


def submit_task(
        circuit,
        task_name=None,
        tasktype=None,
        chip_id=72,
        shots=1000,
        circuit_optimize=True,
        measurement_amend=False,
        auto_mapping=False,
        specified_block=None,
        url=default_submit_url,
        savepath=Path.cwd() / 'online_info',
        **kwargs
):
    '''submit circuits or a single circuit

    Note:
        Actual implementation is _submit_task_group

    Note:
        If wanting compile_only=True, use submit_task_compile_only()

    Args:
        circuits (str or List[str]): Quantum circuit(s) to be submitted.
        task_name (str, optional): The name of the task. Defaults to None.
        tasktype (int): The tasktype. Defaults to None. (Note: reserved field.)
        chip_id (int, optional): The chip id used to identify the quantum chip. Defaults to 72.
        shots (int, optional): Number of shots for every circuit. Defaults to 1000.
        circuit_optimize (bool, optional): Automatically optimize and transpile the circuit. Defaults to True.
        measurement_amend (bool, optional): Amend the measurement result using an internal algorithm. Defaults to True.
        auto_mapping (bool, optional): Automatically select the mapping. Defaults to False.
        specified_block (int, optional): The specified block on chip. Defaults to None. (Note: reserved field.)
        url (str, optional): The URL for submitting the task. Defaults to default_submit_url.
        savepath (str, optional): str. Defaults to Path.cwd()/'online_info'. If None, it will not save the task info.

    Raises:
        RuntimeError: Circuit not input
        RuntimeError: Error when submitting the task

    Returns:
        int: The taskid of this taskgroup
    '''

    if isinstance(circuit, list):
        for c in circuit:
            if not isinstance(c, str):
                raise ValueError('Input is not a valid circuit list (a.k.a List[str]).')

        taskid = _submit_task_group(
            circuits=circuit,
            task_name=task_name,
            tasktype=tasktype,
            chip_id=chip_id,
            shots=shots,
            circuit_optimize=circuit_optimize,
            measurement_amend=measurement_amend,
            auto_mapping=auto_mapping,
            compile_only=False,
            specified_block=specified_block,
            url=url,
            savepath=savepath
        )
    elif isinstance(circuit, str):
        taskid = _submit_task_group(
            circuits=[circuit],
            task_name=task_name,
            tasktype=tasktype,
            chip_id=chip_id,
            shots=shots,
            circuit_optimize=circuit_optimize,
            measurement_amend=measurement_amend,
            auto_mapping=auto_mapping,
            compile_only=False,
            specified_block=specified_block,
            url=url,
            savepath=savepath
        )
    else:
        raise ValueError('Input must be a str or List[str], where each str is a valid originir string.')

    ret = {'taskid': taskid, 'taskname': task_name}
    if savepath:
        make_savepath(savepath)
        with open(savepath / 'online_info.txt', 'a') as fp:
            fp.write(json.dumps(ret) + '\n')

    return taskid


def submit_task_compile_only(
        circuit,
        task_name=None,
        chip_id=72,
        circuit_optimize=True,
        auto_mapping=False,
        url=default_submit_url,
        savepath=Path.cwd() / 'online_info',
        **kwargs
):
    '''Compile a single circuit

    Note:
        Actual implementation is _submit_task_group

    Note:
        If wanting compile_only=False, use submit_task()

    Args:
        circuit (str): A quantum circuit to be submitted.
        task_name (str, optional): The name of the task. Defaults to None.
        tasktype (int): The tasktype. Defaults to None. (Note: reserved field.)
        chip_id (int, optional): The chip id used to identify the quantum chip. Defaults to 72.
        shots (int, optional): Number of shots for every circuit. Defaults to 1000.
        circuit_optimize (bool, optional): Automatically optimize and transpile the circuit. Defaults to True.
        measurement_amend (bool, optional): Amend the measurement result using an internal algorithm. Defaults to True.
        auto_mapping (bool, optional): Automatically select the mapping. Defaults to False.
        specified_block (int, optional): The specified block on chip. Defaults to None. (Note: reserved field.)
        url (str, optional): The URL for submitting the task. Defaults to default_submit_url.
        savepath (str, optional): str. Defaults to Path.cwd()/'online_info'. If None, it will not save the task info.

    Raises:
        RuntimeError: Circuit not input
        RuntimeError: Error when submitting the task

    Returns:
        int: The taskid of this taskgroup
    '''

    if not isinstance(circuit, str):
        raise ValueError('Input is not a valid originir string.')

    return _submit_task_group(
        circuits=[circuit],
        task_name=task_name,
        chip_id=chip_id,
        circuit_optimize=circuit_optimize,
        auto_mapping=auto_mapping,
        compile_only=True,
        url=url,
        savepath=savepath
    )


def query_all_task(url=default_query_url, savepath=None, **kwargs):
    '''Query all task info in the savepath. If you only want to query from taskid, then you can use query_by_taskid instead.

    Args:
        url (str, optional): The url for querying. Defaults to default_query_url.
        savepath (PathLikeObject(str, pathlib.Path, etc...), optional): The savepath for loading the online info. Defaults to None.

    Returns:
        tuple[int,int]: Two integers (finished task count, all task count)
    '''
    if not savepath:
        savepath = Path.cwd() / 'online_info'

    online_info = load_all_online_info(savepath)
    task_count = len(online_info)
    finished = 0
    for task in online_info:
        taskid = task['taskid']

        if isinstance(taskid, list):
            status = 'finished'
            for taskid_i in taskid:
                if not os.path.exists(savepath / '{}.txt'.format(taskid)):
                    taskinfo = query_by_taskid(taskid_i, url)
                    if taskinfo['status'] == 'success' or taskinfo['status'] == 'failed':
                        write_taskinfo(taskid_i, taskinfo, savepath)
                    else:
                        status = 'unfinished'
            if status == 'finished':
                finished += 1

        elif isinstance(taskid, str):
            if not os.path.exists(savepath / '{}.txt'.format(taskid)):
                taskinfo = query_by_taskid(taskid, url)
                if taskinfo['status'] == 'success' or taskinfo['status'] == 'failed':
                    write_taskinfo(taskid, taskinfo, savepath)
                    finished += 1
            else:
                finished += 1
        else:
            raise RuntimeError('Invalid Taskid.')
    return finished, task_count


if __name__ == '__main__':
    result = query_by_taskid_single("55775D4858EF3D2C3813945703243A01")
    print(result)
#     ir = """QINIT 2
# CREG 2
# H q[0]
# H q[1]
# CZ q[0],q[1]
# H q[1]
# MEASURE q[0],c[0]
# MEASURE q[1],c[1]"""
#
#     taskid = submit_task([ir], task_name='test')
#     print(taskid)
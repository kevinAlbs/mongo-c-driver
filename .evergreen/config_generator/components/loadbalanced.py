from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_command import EvgCommandType, FunctionCall, expansions_update
from shrub.v3.evg_task import EvgTaskRef, EvgTaskDependency

from config_generator.components.funcs.bootstrap_mongo_orchestration import BootstrapMongoOrchestration
from config_generator.components.funcs.fetch_build import FetchBuild
from config_generator.components.funcs.fetch_det import FetchDET
from config_generator.components.funcs.run_simple_http_server import RunSimpleHTTPServer
from config_generator.components.funcs.run_tests import RunTests
from config_generator.components.funcs.upload_build import UploadBuild
from config_generator.etc.utils import Task, bash_exec


def functions():
    return {
        'start load balancer': [
            bash_exec(
                command_type=EvgCommandType.SETUP,
                script='''\
                    export DRIVERS_TOOLS=./drivers-evergreen-tools
                    export MONGODB_URI="${MONGODB_URI}"
                    $DRIVERS_TOOLS/.evergreen/run-load-balancer.sh start
                ''',
            ),
            expansions_update(
                command_type=EvgCommandType.SETUP,
                file='lb-expansion.yml',
            )
        ]
    }


def make_test_task(auth: bool, ssl: bool, server_version: str):
    auth_str = "auth" if auth else "noauth"
    ssl_str = "ssl" if ssl else "nossl"
    return Task(
        name=f"loadbalanced-test-{auth_str}-{ssl_str}-{server_version}",
        depends_on=[EvgTaskDependency(name="loadbalanced-compile")],
        # Use `rhel8.7` distro. `rhel8.7` distro includes necessary dependency: `haproxy`.
        run_on="rhel8.7-small",
        tags=['loadbalanced'],
        commands=[
            FetchBuild.call(build_name='loadbalanced-compile'),
            FetchDET.call(),
            BootstrapMongoOrchestration().call(vars={
                'AUTH': auth_str,
                'SSL': ssl_str,
                'MONGODB_VERSION': server_version,
                'TOPOLOGY': 'sharded_cluster',
                'LOAD_BALANCER': 'on',
            }),
            RunSimpleHTTPServer.call(),
            FunctionCall(func='start load balancer', vars={
                'MONGODB_URI': 'mongodb://localhost:27017,localhost:27018'
            }),
            RunTests().call(vars={
                'AUTH': auth_str,
                'SSL': ssl_str,
                'LOADBALANCED': 'loadbalanced',
                'CC': 'gcc',
            })
        ],
    )


def tasks():
    yield Task(
        name="loadbalanced-compile",
        run_on="rhel8.7-large",
        tags=['loadbalanced'],
        commands=[
            bash_exec(
                command_type=EvgCommandType.TEST,
                env={
                    'CC': 'gcc',
                    'CFLAGS': '-fno-omit-frame-pointer',
                    'EXTRA_CONFIGURE_FLAGS': '-DENABLE_EXTRA_ALIGNMENT=OFF',
                    'SSL': 'OPENSSL'
                },
                working_dir='mongoc',
                script='.evergreen/scripts/compile.sh',
            ),
            UploadBuild.call()
        ],
    )

    # Satisfy requirements specified in
    # https://github.com/mongodb/specifications/blob/14916f76fd92b2686d8e3d1f0e4c2d2ef88ca5a7/source/load-balancers/tests/README.rst#testing-requirements
    #
    # > For each server version that supports load balanced clusters, drivers
    # > MUST add two Evergreen tasks: one with a sharded cluster with both
    # > authentication and TLS enabled and one with a sharded cluster with
    # > authentication and TLS disabled.
    server_versions = ['5.0', '6.0', '7.0', 'latest']
    for server_version in server_versions:
        yield make_test_task(auth=False, ssl=False, server_version=server_version)
        yield make_test_task(auth=True, ssl=True, server_version=server_version)


def variants():
    return [
        BuildVariant(
            name="loadbalanced",
            display_name="loadbalanced",
            tasks=[EvgTaskRef(name='.loadbalanced')]
        ),
    ]
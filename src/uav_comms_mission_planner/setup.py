from glob import glob

from setuptools import find_packages, setup

package_name = 'uav_comms_mission_planner'

setup(
    name=package_name,
    version='1.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/launch', glob('launch/*.launch.py')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='efe',
    maintainer_email='efecekic0@gmail.com',
    description='Mission goal and swarm membership functions for the DroneComs swarm(s).',
    license='AGPL-3.0',
    entry_points={
        'console_scripts': [
            'mission_planner = uav_comms_mission_planner.mission_planner:main',
            'swarm_registry = uav_comms_mission_planner.swarm_registry:main',
        ],
    },
)

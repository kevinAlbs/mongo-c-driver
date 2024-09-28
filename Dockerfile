FROM alpine:3.19
RUN apk add python3 bash
COPY .evergreen/config_generator /s/.evergreen/config_generator
COPY .evergreen/legacy_config_generator /s/.evergreen/legacy_config_generator
COPY tools /s/tools
COPY pyproject.toml /s
COPY poetry.lock /s
WORKDIR /s
# RUN ./tools/poetry.sh install --with=dev
# TODO: mc-evg-generate fails with:
# """
# pydantic_core._pydantic_core.ValidationError: 2 validation errors for EvgProject
# """
# RUN ./tools/poetry.sh run mc-evg-generate 

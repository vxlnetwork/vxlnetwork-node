FROM vxlnetworkcurrency/vxlnetwork-env:gcc

ARG NETWORK=live
ARG TRAVIS_TAG=DEV_BUILD
ARG CI_BUILD=OFF
ADD ./ /tmp/src

RUN mkdir /tmp/build && \
cd /tmp/build && \
cmake /tmp/src -DCI_BUILD=${CI_BUILD} \
-DBOOST_ROOT=${BOOST_ROOT} -DPORTABLE=1 \
-DACTIVE_NETWORK=vxlnetwork_${NETWORK}_network \
-DVXLNETWORK_POW_SERVER=ON -DVXLNETWORK_SHARED_BOOST=ON && \
make vxlnetwork_node -j $(nproc) && \
make vxlnetwork_rpc -j $(nproc) && \
make vxlnetwork_pow_server -j $(nproc) && \
cd .. && \
echo ${NETWORK} >/etc/vxlnetwork-network

FROM ubuntu:18.04

RUN groupadd --gid 1000 vxlnetworkcurrency && \
useradd --uid 1000 --gid vxlnetworkcurrency --shell /bin/bash --create-home vxlnetworkcurrency

COPY --from=0 /tmp/build/vxlnetwork_node /usr/bin
COPY --from=0 /tmp/build/vxlnetwork_rpc /usr/bin
COPY --from=0 /tmp/build/vxlnetwork_pow_server /usr/bin
COPY --from=0 /tmp/src/api/ /usr/bin/api/
COPY --from=0 /etc/vxlnetwork-network /etc
COPY docker/node/entry.sh /usr/bin/entry.sh
COPY docker/node/config /usr/share/vxlnetwork/config
COPY --from=0 /tmp/boost/lib/* /usr/local/lib/
RUN chmod +x /usr/bin/entry.sh
RUN ln -s /usr/bin/vxlnetwork_node /usr/bin/rai_node
RUN ldconfig

WORKDIR /root
USER root

ENV PATH="${PATH}:/usr/bin"
ENTRYPOINT ["/usr/bin/entry.sh"]
CMD ["vxlnetwork_node", "daemon", "-l"]
ARG REPOSITORY=vxlnetworkcurrency/vxlnetwork-node
LABEL org.opencontainers.image.source https://github.com/$REPOSITORY
